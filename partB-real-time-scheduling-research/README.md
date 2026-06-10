# Real-Time Scheduling Mechanism Research

## Implemented Scheduling Algorithms

| Algorithm | Abbr | Type | Feature |
|-----------|------|------|---------|
| Rate Monotonic Scheduling | **RMS** | Static priority / Preemptive | Shorter period = higher priority, optimal fixed-priority algorithm |
| Earliest Deadline First | **EDF** | Dynamic priority / Preemptive | Optimal uniprocessor dynamic priority algorithm, U<=1 schedulable |
| Least Laxity First | **LLF** | Dynamic priority / Preemptive | Laxity = deadline - current_time - remaining_execution_time |
| Deadline Monotonic Scheduling | **DMS** | Static priority / Preemptive | Generalization of RMS, for constrained deadline tasks |
| First Come First Serve | **FCFS** | Non-preemptive | Baseline algorithm for comparison |
| Shortest Job First | **SJF** | Non-preemptive | Baseline algorithm for comparison |

## Schedulability Analysis Support

- **RMS**: Liu & Layland bound, Hyperbolic bound (Bini), Response Time Analysis (RTA)
- **EDF**: Utilization test, Density test, Processor Demand Criterion (PDC)
- **DMS**: Response Time Analysis (RTA)

## Project Structure

```
real-time-scheduling-research/
├── include/                 # Headers
│   ├── rt_types.h          # Core type definitions
│   ├── task.h              # Task management interface
│   └── scheduler.h         # Scheduler interface & schedulability analysis
├── src/
│   ├── main.c              # Main entry (10 experiments)
│   ├── task.c              # Task lifecycle & task set generation
│   ├── simulator.c         # Discrete event simulation engine
│   ├── analysis.c          # Schedulability analysis implementation
│   └── schedulers/         # Scheduler implementations
│       ├── rms.c           # RMS + RTA
│       ├── edf.c           # EDF + PDC
│       ├── llf.c           # LLF
│       ├── dms.c           # DMS + RTA
│       └── fcfs.c          # FCFS + SJF
├── visualization/
│   └── plot.py             # Python visualization script (matplotlib)
├── results/                # Experiment results (auto-generated)
├── Makefile                # One-click build and run
└── README.md               # This file
```

## Quick Start

### Requirements

- GCC compiler
- Make tool
- Python 3 + matplotlib (for visualization)

### One-click run

```bash
cd real-time-scheduling-research
make run
```

This command will:
1. Compile all source files
2. Run all 10 experiments
3. Generate CSV result data
4. Call Python script to generate visualization plots

### Individual commands

```bash
make        # Compile only
make plot   # Generate plots (requires existing results)
make clean  # Clean all generated files
make help   # Show help
```

## Experiments

### Exp 1: Basic Scheduling Demo
Show RMS and EDF scheduling process with 3 tasks, output Gantt chart data.

### Exp 2: Algorithm Comparison
Run all 6 schedulers on the same random task set, compare performance metrics.

### Exp 3: Schedulability Rate Test
Generate大量 random task sets at different utilization levels,统计 schedulability rates.

### Exp 4: Overload Scenario
Test behavior when utilization exceeds 100%, analyze deadline miss ratio.

### Exp 5: Scalability Test
Analyze performance trends as task count increases from 2 to 20.

### Exp 6: Boundary Case Analysis
Construct classic case where RMS is unschedulable but EDF is schedulable.

### Exp 7: Context Switch Analysis
Compare context switch counts and preemption counts across algorithms.

### Exp 8: Period Sensitivity
Analyze impact of task period changes on scheduling performance.

### Exp 9: Unit Tests
16 unit tests covering task management, task sets, RMS/EDF schedulability, simulator, utilities.

### Exp 10: Performance Benchmark
Compare execution efficiency of 6 schedulers on same large task set.

## Key Technical Details

### Discrete Event Simulation Engine
- Fixed time-step discrete event simulation
- Precise handling of task arrival, execution, completion, deadline miss events
- Unified scheduler interface (function pointers for polymorphism)

### Task Set Generation
- **UUniFast algorithm**: Uniformly generate task sets with specified total utilization
- **RMS-friendly**: Power-of-2 periods
- **EDF-critical**: Construct task sets with utilization close to 1

### Visualization Output
- Gantt chart: Visualize task scheduling timeline
- Bar chart: Multi-algorithm multi-metric comparison
- Line chart: Schedulability rate vs utilization
- Scatter/line charts: Scalability, overload analysis

## Author

- **Name**: Wang Yufan
- **Course**: Operating Systems
- **Content**: Real-Time Scheduling Mechanism Research
- **Date**: 2026-06-06
