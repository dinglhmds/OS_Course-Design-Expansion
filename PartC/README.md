# CPU Scheduling Simulator

A lightweight Python framework for simulating and benchmarking CPU scheduling algorithms.

Supports three execution models:
- **Batch mode** — classic non-realtime scheduling (FCFS, RR, Priority, SJF)
- **Realtime mode** — periodic tasks with deadline constraints (EDF, RMS)
- **Multicore mode** — multi-core scheduling with migration penalties (GlobalRR)

---

## Quick Start

```bash
# Run batch benchmark
python main.py --mode batch --tasks 8 --seed 42

# Run realtime benchmark
python main.py --mode realtime --duration 60

# Run multicore benchmark
python main.py --mode multicore --num-cpus 2 --quantum 4
```

Requirements: Python 3.10+ (optional: `pip install matplotlib` for charts)

---

## Project Structure

```
.
├── interfaces.py          # Task class + BaseScheduler abstract base class
├── engine.py              # Simulation engine (batch / realtime / multicore)
├── metrics.py             # Performance metrics (TAT, fairness, miss rate, speedup)
├── report_generator.py    # CSV export + Gantt chart plotting
├── main.py                # CLI entry point with three modes
└── algorithms/
    ├── fcfs.py            # First-Come-First-Served
    ├── rr.py              # Round-Robin
    ├── priority.py        # Non-preemptive Priority
    ├── custom_demo.py     # Shortest-Job-First (example custom algorithm)
    ├── edf.py             # Earliest Deadline First (realtime)
    ├── rms.py             # Rate Monotonic Scheduling (realtime)
    └── multi_core.py      # Global Round-Robin (multicore)
```

---

## How to Design a New Algorithm

The framework uses a **plugin architecture**. You only need to inherit `BaseScheduler` and implement two methods.

### Step 1: Create a new file

Create `algorithms/my_algo.py`:

```python
from interfaces import BaseScheduler, Task
from typing import Dict, Optional

class MyAlgoScheduler(BaseScheduler):
    def __init__(self):
        super().__init__("MyAlgo")
        # Initialize your data structures here

    def add_task(self, task: Task) -> None:
        """Called by the engine when a new task arrives."""
        self.ready_queue.append(task)

    def schedule(self, current_time: int, cpu_status_dict: Dict[int, Optional[Task]]) -> Dict[int, Optional[Task]]:
        """
        Core scheduling decision.

        Parameters:
            current_time    — current simulation clock tick
            cpu_status_dict — {cpu_id: currently_running_task_or_None}

        Returns:
            {cpu_id: task_to_run_or_None} for the upcoming tick
        """
        # Single-core example: just pick the first task in queue
        if self.ready_queue:
            task = self.ready_queue.pop(0)
            return {0: task}
        return {0: None}
```

### Step 2: Register in `main.py`

Import your scheduler and add it to the benchmark list:

```python
from algorithms.my_algo import MyAlgoScheduler

# In run_batch_mode():
algorithms = [
    FCFSScheduler(),
    RRScheduler(),
    MyAlgoScheduler(),   # <-- add here
]
```

### Step 3: Run and compare

```bash
python main.py --mode batch
```

The framework automatically runs your algorithm alongside the baselines on the **same task stream** and prints a comparison table.

---

## Key Concepts

### Single-Core vs Multi-Core

| | Single-Core | Multi-Core |
|:---|:---|:---|
| Return value | `{0: task}` | `{0: t0, 1: t1, ...}` |
| Migration | N/A | If a task moves from CPU A to CPU B, the engine adds `remaining_time += 1` as a cache-miss penalty |

### Batch vs Realtime

| | Batch | Realtime |
|:---|:---|:---|
| Task source | One-shot list | Periodic generation (`period`, `deadline`, `wcet`) |
| Key metric | Turnaround time / Fairness | Deadline miss rate / Response time |
| Engine | `run_simulation()` | `run_realtime_simulation()` |

---

## Testing Your Algorithm

### Test 1: Batch performance

```bash
python main.py --mode batch --tasks 10 --seed 123 --quantum 3
```

Output: comparison table with Avg_TAT, Avg_WT, Fairness_Jain, Throughput.

### Test 2: Realtime deadline compliance

```bash
python main.py --mode realtime --duration 60
```

Output: deadline miss rate, max response time, jitter.

### Test 3: Multi-core scalability

```bash
python main.py --mode multicore --num-cpus 4 --quantum 5
```

Output: speedup, migration count.

### Custom quick test (no CLI)

```python
from engine import run_simulation
from interfaces import Task
from metrics import analyze_results
from algorithms.my_algo import MyAlgoScheduler

tasks = [Task(pid=1, arrival_time=0, burst_time=5),
         Task(pid=2, arrival_time=1, burst_time=3)]

scheduler = MyAlgoScheduler()
completed, gantt, total_time = run_simulation(scheduler, tasks)
metrics = analyze_results(completed, total_time)
print(metrics)
```

---

## Metrics Reference

| Metric | Description |
|:---|:---|
| **Avg Turnaround Time** | `completion - arrival`, averaged over all tasks |
| **Avg Waiting Time** | Time spent in ready queue |
| **Fairness Jain** | 1.0 = perfectly fair (equal CPU time share) |
| **Deadline Miss Rate** | `missed_jobs / total_jobs` (realtime) |
| **Max Response Time** | Worst-case `completion - arrival` (realtime) |
| **Jitter** | Std-dev of response times (realtime stability) |
| **Speedup** | `sum(burst) / (total_time * num_cpus)` (multicore efficiency) |
| **Migration Count** | Number of cross-core task migrations |

---

## Tips for Algorithm Design

1. **Keep tasks in `ready_queue`** — the engine expects `scheduler.ready_queue` to hold waiting tasks.
2. **Filter completed tasks** — before selecting a task, remove those with `remaining_time <= 0`.
3. **Handle empty queue** — return `{0: None}` (or `{cpu_id: None}`) when no task is available.
4. **Migration-aware** — in multicore mode, try to keep tasks on the same CPU to avoid the `+1` penalty.
