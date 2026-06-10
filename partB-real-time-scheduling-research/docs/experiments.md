# Experiment Design

## Overview

This system contains 10 core experiments covering scheduling demonstration, algorithm comparison, schedulability analysis, overload testing, scalability analysis, boundary cases, context switch analysis, period sensitivity analysis, unit tests, and performance benchmarks.

---

## Exp 1: Basic Scheduling Demonstration

**Purpose**: Understand RMS and EDF scheduling behavior differences

**Task Set**:
| Task | Period | Execution Time | Utilization |
|------|--------|----------------|-------------|
| T1-high | 10 | 3 | 0.30 |
| T2-mid | 20 | 5 | 0.25 |
| T3-low | 30 | 6 | 0.20 |
| **Total** | | | **0.75** |

**Output**: Gantt chart CSV data, visualizable with Python

**Expected Result**: Both RMS and EDF schedule correctly, but with different orders

---

## Exp 2: Algorithm Comparison

**Purpose**: Compare all 6 schedulers on the same task set

**Method**: Generate random task set using UUniFast (5 tasks, target U=0.75)

**Metrics**:
- Completed jobs
- Missed deadlines
- Deadline miss ratio
- Preemption count
- Context switch count
- CPU utilization
- Average/max response time

---

## Exp 3: Schedulability Rate Test

**Purpose**: Verify theoretical schedulability bounds

**Method**:
1. For each utilization U (0.1 ~ 1.0, step 0.05)
2. Generate 100 random task sets
3. Calculate schedulable proportion for each algorithm

**Tests**:
- RMS: LL bound, Hyperbolic bound, RTA
- EDF: Utilization test, Density test
- DMS: RTA

**Expected Trends**:
- Lower utilization → Higher schedulability rate
- EDF rate >= DMS rate >= RMS rate
- LL bound is most conservative (lowest rate), RTA is most precise

---

## Exp 4: Overload Scenario

**Purpose**: Analyze behavior when utilization exceeds 100%

**Method**:
1. Utilization from 0.6 to 1.4 (step 0.1)
2. Run all schedulers at each utilization level
3. Record miss ratio and preemption count

**Expected Results**:
- EDF shows domino effect under overload
- LLF context switches increase dramatically
- Fixed-priority algorithms (RMS/DMS) degrade more gracefully

---

## Exp 5: Scalability Test

**Purpose**: Analyze impact of task count on scheduling performance

**Method**:
1. Task count from 2 to 20 (step 2)
2. Keep total utilization around 0.7
3. Record response time and miss ratio

---

## Exp 6: Boundary Case Analysis

**Purpose**: Construct case where RMS is unschedulable but EDF is schedulable

**Task Set Design**:
| Task | Period | Execution Time | Utilization |
|------|--------|----------------|-------------|
| T1-fast | 4 | 2 | 0.50 |
| T2-mid | 10 | 3 | 0.30 |
| T3-slow | 20 | 4 | 0.20 |
| **Total** | | | **1.00** |

**Theory**:
- RMS bound (n=3): 0.7798 < 1.0, LL test fails
- EDF utilization test: 1.0 <= 1.0, passes
- Exact RTA may show RMS is actually schedulable (another case)

---

## Exp 7: Context Switch Analysis

**Purpose**: Quantify context switch overhead across algorithms

**Method**: Use high-frequency task set to generate many scheduling decisions

**Key Observations**:
- LLF has most context switches due to churning
- Non-preemptive algorithms (FCFS/SJF) have fewest switches
- EDF usually slightly fewer than RMS (reduces unnecessary preemptions)

---

## Exp 8: Period Sensitivity

**Purpose**: Analyze impact of task period ratio on scheduling performance

**Method**:
1. Fix two tasks, keep total utilization constant
2. Adjust period ratio (0.5 ~ 3.0)
3. Observe response time and miss ratio changes

---

## Exp 9: Unit Tests

**Purpose**: Verify correctness of core code

**Coverage**:
- Task management: init, utilization, execute, deadline miss detection
- Task sets: utilization sum, RMS bound calculation
- RMS schedulability: LL pass case, LL fail case, RTA exact
- EDF schedulability: Utilization test
- Simulator: RMS basic, EDF basic, EDF overload
- Utilities: GCD/LCM, rounding

**Output**: Test report CSV recording category, name, and result

---

## Exp 10: Performance Benchmark

**Purpose**: Compare execution efficiency of scheduling algorithms

**Method**:
1. Use 10-task random task set (U=0.8)
2. Run all 6 schedulers on same hardware
3. Record simulation execution time

**Key Metrics**:
- Execution time (ms)
- Relative speed factor (first scheduler as baseline)

---

## Data Format

All results saved as CSV in `results/`:

### Generic format
```csv
scheduler,completed,missed,miss_ratio,preemptions,context_switches,cpu_util,...
```

### Gantt chart format
```csv
task_id,task_name,start,end,duration,state
```

### Schedulability format
```csv
utilization,rms_ll,rms_hyp,rms_rta,edf_util,edf_density,dms_rta
```

---

## Visualization

Use `visualization/plot.py` to auto-generate:

| Experiment | Chart Type | Filename |
|------------|------------|----------|
| Exp 1 | Gantt chart | exp1_*_gantt.png |
| Exp 2 | Multi-metric bar chart | exp2_comparison.png |
| Exp 3 | Schedulability line chart | exp3_schedulability.png |
| Exp 4 | Dual Y-axis line chart | exp4_overload.png |
| Exp 5 | Scalability line chart | exp5_scalability.png |
| Exp 6 | Multi-metric bar chart | exp6_boundary.png |
| Exp 7 | Bar chart comparison | exp7_context_switch.png |
| Exp 8 | Sensitivity line chart | exp8_period_sensitivity.png |
