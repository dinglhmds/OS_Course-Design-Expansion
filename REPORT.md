# CPU 调度仿真框架设计与实现报告

## 一、项目概述

本项目实现了一个 **CPU Scheduling Simulator Framework**，用于快速评估和对比各类 CPU 调度算法。该框架支持三种仿真模式：批处理调度（Batch）、实时调度（Realtime）和多核调度（Multicore），并提供了完整的性能指标计算与可视化功能。

**项目地址**：https://github.com/dinglhmds/sched-simulator

---

## 二、项目目标

1. **算法快速验证**：开发者仅需实现 2-3 个方法即可接入自定义调度算法
2. **公平对比**：同一组任务流在多个算法上运行，确保对比的客观性
3. **多场景覆盖**：支持批处理、实时周期任务、多核并发三种典型场景
4. **可视化输出**：自动生成甘特图和 CSV 报告，直观展示调度效果

---

## 三、系统架构

### 3.1 目录结构

```
sched-simulator/
├── interfaces.py          # 核心接口：Task 数据类 + BaseScheduler 抽象基类
├── engine.py              # 仿真引擎：离散事件驱动，支持三种模式
├── metrics.py             # 指标计算：批处理/实时/多核三类指标
├── report_generator.py    # 报告生成：CSV 导出 + Matplotlib 甘特图
├── main.py                # CLI 入口：参数解析 + 三种模式调度
├── README.md              # 使用文档
└── algorithms/            # 算法实现目录
    ├── fcfs.py            # 先来先服务（FCFS）
    ├── rr.py              # 时间片轮转（Round-Robin）
    ├── priority.py        # 优先级调度（非抢占式）
    ├── custom_demo.py     # 短作业优先（SJF，自定义算法示例）
    ├── edf.py             # 最早截止期限优先（EDF，实时）
    ├── rms.py             # 速率单调调度（RMS，实时）
    └── multi_core.py      # 全局轮转调度（GlobalRR，多核）
```

### 3.2 核心设计

#### Task 数据类

```python
@dataclass
class Task:
    pid: int                    # 任务标识
    arrival_time: int           # 到达时间
    burst_time: int             # 执行时间
    priority: int = 0           # 优先级
    remaining_time: int         # 剩余执行时间（运行时更新）
    
    # 实时扩展
    period: Optional[int]       # 周期
    deadline: Optional[int]     # 相对截止期限
    wcet: Optional[int]         # 最坏执行时间
    absolute_deadline: int      # 绝对截止期限
    missed_deadline: bool       # 是否超时
    response_time: int          # 响应时间
    
    # 多核扩展
    last_cpu: int = -1          # 上次运行的 CPU
    migration_count: int = 0    # 迁移次数
    
    # 统计信息
    start_time: Optional[int]
    completion_time: Optional[int]
    turnaround_time: Optional[int]
    waiting_time: int = 0
```

#### BaseScheduler 抽象基类

```python
class BaseScheduler(ABC):
    def __init__(self, name: str):
        self.name = name
        self.ready_queue: list[Task] = []
        self.current_task: Optional[Task] = None

    @abstractmethod
    def add_task(self, task: Task) -> None:
        """新任务到达时的处理逻辑"""
        ...

    def schedule(self, current_time, cpu_status_dict) -> Dict[int, Optional[Task]]:
        """核心调度决策，返回 {cpu_id: task_or_None}"""
        ...
```

**设计要点**：
- `schedule()` 返回字典 `{cpu_id: task}`，天然支持多核扩展
- 单核算法可简化为返回 `{0: task}`
- 引擎自动处理迁移检测和缓存惩罚

---

## 四、三种仿真模式

### 4.1 Batch 模式（批处理调度）

**场景**：操作系统作业队列，任务一次性到达，追求平均周转时间和公平性。

**引擎逻辑**：
1. 按到达时间排序任务列表
2. 每个时间单位检查新到达任务，调用 `add_task()`
3. 调用 `schedule()` 获取调度决策
4. 执行任务（`remaining_time -= 1`）
5. 更新就绪队列中任务的等待时间
6. 任务完成时记录统计信息

**实现的算法**：

| 算法 | 类型 | 核心策略 |
|:---|:---|:---|
| FCFS | 非抢占 | FIFO 队列，先到先服务 |
| RR | 抢占 | 时间片轮转，quantum 到期强制切换 |
| Priority | 非抢占 | 按优先级排序，高优先级优先 |
| SJF | 非抢占 | 按 burst_time 排序，短作业优先 |

### 4.2 Realtime 模式（实时调度）

**场景**：嵌入式实时系统，任务按周期到达，必须在截止期限前完成。

**引擎逻辑**：
1. 根据 `period` 在每个周期边界自动生成新任务实例（Job）
2. 调用 `schedule()` 进行调度决策
3. 任务完成时检查 `completion_time > absolute_deadline`
4. 标记超时任务，记录响应时间

**实现的算法**：

| 算法 | 优先级类型 | 核心策略 | 适用场景 |
|:---|:---|:---|:---|
| EDF | 动态 | 绝对截止期限最小的优先 | 软实时，负载变化大 |
| RMS | 静态 | 周期越短优先级越高 | 硬实时，周期固定 |

**关键特性**：
- EDF 是单核最优动态优先级算法（Liu & Layland 证明）
- RMS 在利用率低于 `n*(2^(1/n)-1)` 时可保证所有截止期限满足

### 4.3 Multicore 模式（多核调度）

**场景**：多核处理器，需要负载均衡和缓存亲和性优化。

**引擎逻辑**：
1. 维护每个 CPU 核心的运行状态 `cpu_running[cpu_id]`
2. 调用 `schedule(current_time, cpu_status_dict)` 获取多核分配方案
3. **迁移检测**：若任务从 CPU A 切换到 CPU B，施加 `remaining_time += 1` 惩罚
4. 记录迁移次数用于后续分析

**实现的算法**：

| 算法 | 核心策略 |
|:---|:---|
| GlobalRR | 全局就绪队列，空闲 CPU 从队列头部取任务，时间片到期后重新分配 |

**迁移惩罚机制**：
```python
if assigned.last_cpu != -1 and assigned.last_cpu != cpu_id:
    assigned.remaining_time += 1  # 模拟缓存失效开销
    assigned.migration_count += 1
```

---

## 五、性能指标体系

### 5.1 批处理指标

| 指标 | 公式 | 意义 |
|:---|:---|:---|
| 平均周转时间 | `Σ(completion - arrival) / n` | 任务从提交到完成的平均时间 |
| 平均等待时间 | `Σwaiting_time / n` | 任务在就绪队列中的平均等待 |
| 平均带权周转时间 | `Σ(turnaround / burst) / n` | 归一化的周转时间 |
| 公平性标准差 | `stddev(waiting_times)` | 等待时间的离散程度 |
| Jain 公平指数 | `(Σwt)² / (n * Σwt²)` | 1.0 = 完全公平 |
| 吞吐量 | `n / total_time` | 单位时间完成的任务数 |

### 5.2 实时指标

| 指标 | 公式 | 意义 |
|:---|:---|:---|
| 截止期限错失率 | `missed_jobs / total_jobs` | 实时系统最核心的可靠性指标 |
| 最大响应时间 | `max(completion - arrival)` | 最坏情况下的延迟 |
| 平均响应时间 | `Σ(completion - arrival) / n` | 典型延迟水平 |
| 抖动 | `stddev(response_times)` | 响应时间的稳定性 |

### 5.3 多核指标

| 指标 | 公式 | 意义 |
|:---|:---|:---|
| 加速比 | `Σburst / (total_time * num_cpus)` | 接近 1.0 表示完美并行 |
| 迁移次数 | `Σmigration_count` | 跨核迁移的总次数 |
| 平均迁移次数 | `migration_count / n` | 每个任务的平均迁移 |

---

## 六、实验结果与分析

### 6.1 Batch 模式对比

**测试配置**：8 个随机任务，seed=42

| 算法 | 总耗时 | 平均周转时间 | 平均等待时间 | Jain 公平指数 |
|:---|:---|:---|:---|:---|
| **SJF** | 48 | **21.38** | **15.38** | 0.6138 |
| Priority | 48 | 23.62 | 17.62 | 0.6963 |
| FCFS | 48 | 25.25 | 19.25 | 0.7068 |
| RR(q=4) | 48 | 34.38 | 28.38 | 0.9094 |
| RR(q=2) | 48 | 35.38 | 29.38 | **0.9445** |

**分析**：
- **SJF** 在平均周转时间上最优，因为短作业优先减少了长任务对短任务的阻塞
- **RR** 公平性指数最高（接近 1.0），但周转时间最差，时间片越小公平性越好但切换开销越大
- **FCFS** 实现最简单，但对短作业不友好（护航效应）

### 6.2 Realtime 模式对比（正常负载）

**测试配置**：T1(period=5, wcet=2), T2(period=10, wcet=3), T3(period=15, wcet=3)，duration=60，利用率 80%

| 算法 | Jobs | Missed | Miss Rate | Avg Response Time |
|:---|:---|:---|:---|:---|
| FCFS | 22 | 0 | 0.00% | 3.91 |
| RR(q=2) | 22 | 0 | 0.00% | 4.73 |
| **EDF** | 22 | 0 | 0.00% | **3.82** |
| **RMS** | 22 | 0 | 0.00% | **3.82** |

**分析**：
- 利用率 80% 时所有算法都能满足截止期限
- EDF 和 RMS 响应时间最优，体现了实时调度算法的优势

### 6.3 Realtime 模式对比（过载）

**测试配置**：T1(period=5, wcet=3), T2(period=10, wcet=4), T3(period=15, wcet=5)，利用率 133%

| 算法 | Jobs | Missed | **Miss Rate** | Avg Response Time |
|:---|:---|:---|:---|:---|
| **RMS** | 22 | 3 | **13.64%** | **5.33** |
| EDF | 22 | 15 | 68.18% | 12.88 |
| FCFS | 22 | 15 | 68.18% | 13.25 |
| RR(q=2) | 22 | 19 | **86.36%** | 18.31 |

**分析**：
- **RMS 碾压级优势**：静态优先级保护短周期任务，仅 13.64% miss
- **RR 最差**：完全不考虑截止期限，86% 的任务超时
- **EDF 退化**：在严重过载时，EDF 的动态优先级无法区分关键任务

### 6.4 Multicore 模式验证

**测试配置**：2 CPUs，4 个任务，burst=10

| 算法 | CPUs | 总时间 | 加速比 | 迁移次数 |
|:---|:---|:---|:---|:---|
| GlobalRR(q=10) | 2 | 20 | **1.0000** | 0 |
| GlobalRR(q=4) | 2 | 20 | **1.0000** | 0 |

**分析**：
- 总工作量 40，2 核并行理论最小时间 = 20
- 实际达到 20，加速比 1.0，说明完美并行
- 无迁移（quantum ≥ burst，每个任务一次性完成）

---

## 七、关键技术实现

### 7.1 向后兼容设计

`BaseScheduler.schedule()` 的默认实现调用 `next_task()`，使得原有的单核算法无需修改即可在新引擎上运行：

```python
def schedule(self, current_time, cpu_status_dict):
    task = self.next_task(current_time)
    return {0: task}
```

### 7.2 甘特图多格式自适应

`report_generator.py` 自动检测 gantt 数据的格式：
- `(pid, start, end)` — 单核批处理
- `(pid, start, end, missed)` — 单核实时
- `(pid, start, end, cpu_id)` — 多核批处理
- `(pid, start, end, missed, cpu_id)` — 多核实时

### 7.3 离散事件模拟

引擎采用**时间片步进**而非事件驱动，简化了实现：
- 每个时间单位执行：到达检查 → 调度决策 → 执行 → 完成检查
- 适合教学和理解，牺牲部分效率换取可读性

---

## 八、使用指南

### 8.1 快速开始

```bash
# 批处理模式
python main.py --mode batch --tasks 8 --seed 42

# 实时模式
python main.py --mode realtime --duration 60

# 多核模式
python main.py --mode multicore --num-cpus 2 --quantum 4
```

### 8.2 接入自定义算法

1. 创建 `algorithms/my_algo.py`，继承 `BaseScheduler`
2. 实现 `add_task()` 和 `schedule()`
3. 在 `main.py` 中导入并加入算法列表

完整示例见 README.md。

---

## 九、总结与展望

### 9.1 已完成工作

- ✅ 统一的调度算法接口（`BaseScheduler`）
- ✅ 三种仿真模式（Batch / Realtime / Multicore）
- ✅ 7 个预制算法（FCFS, RR, Priority, SJF, EDF, RMS, GlobalRR）
- ✅ 完整的性能指标体系
- ✅ 自动报告生成（CSV + 甘特图）

### 9.2 可扩展方向

1. **能耗模型**：增加 CPU 频率调节，模拟 DVFS 节能调度
2. **缓存模拟**：更精细的缓存层次模型，替代简单的 `+1` 惩罚
3. **优先级继承**：实现实时系统的优先级天花板/继承协议
4. **分布式调度**：扩展到 NUMA 架构，考虑内存亲和性
5. **ML 调度器**：用强化学习训练调度策略，与传统算法对比