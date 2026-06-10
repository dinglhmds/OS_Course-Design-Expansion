# Part A: Scheduler Improvement and Optimization

This module implements the expansion topic **Scheduling Algorithm Improvement and Optimization** for the Operating System coursework.

It is designed as a reproducible Linux/WSL experiment rather than a single hard-coded demo. The project contains a C11 CPU scheduling simulator, classical schedulers, a classic MLFQ baseline, an improved MLFQ scheduler, a reinforcement-learning scheduler, multiple workloads, unified tests, batch benchmarks, and generated SVG charts.

## Main Contributions

- A unified discrete-event CPU scheduling simulator in C11.
- Six traditional schedulers: `fcfs`, `sjf`, `srtf`, `rr`, `priority`, and classic `mlfq`.
- One improved scheduler: `aamlfq`, an Adaptive Aging MLFQ variant.
- One explicit reinforcement-learning scheduler: `qlearn`.
- Nine workloads covering CPU-bound, I/O-heavy, deadline-heavy, short-burst, starvation-risk, and larger mixed scenarios.
- Unified correctness tests and reproducible benchmark scripts.
- Automatically generated CSV reports and SVG charts.

## Implemented Algorithms

- `fcfs`: first-come, first-served baseline.
- `sjf`: non-preemptive shortest-job-first baseline.
- `srtf`: preemptive shortest-remaining-time-first baseline.
- `rr`: round-robin baseline with configurable time quantum.
- `priority`: preemptive priority scheduling with waiting-time aging.
- `mlfq`: classic Multi-Level Feedback Queue scheduling.
- `aamlfq`: improved Adaptive Aging Multi-Level Feedback Queue scheduling.
- `qlearn`: online tabular Q-learning scheduler.

## Classic MLFQ Scheduler

The `mlfq` scheduler is the classic **Multi-Level Feedback Queue** scheduler:

- all new processes enter the highest-priority queue;
- the scheduler always chooses a runnable process from the highest non-empty queue;
- processes at the same queue level are selected by ready-queue order;
- lower-priority queues receive longer quanta;
- if a process uses up its time quantum and is not finished, it is demoted one level;
- if a process blocks for I/O before finishing its quantum, it returns to the same queue level;
- no aging promotion, global boost, I/O bonus, or deadline-aware promotion is used.

## Improved AAMLFQ Scheduler

The `aamlfq` scheduler adds optimization mechanisms on top of classic MLFQ:

- **aging promotion**: a process waiting too long is promoted upward;
- **periodic global boost**: ready processes are periodically moved back to the top queue;
- **I/O return bonus**: I/O-bound processes return one level higher when possible;
- **deadline slack awareness**: urgent soft-deadline processes are temporarily treated as top-level candidates;
- **deadline tie-break**: when queue level is equal, earlier deadlines are preferred.

This version is intentionally separate from `mlfq`, so the benchmark can compare the classical algorithm and the improved algorithm directly.

## Q-Learning Scheduler

The `qlearn` scheduler is a reinforcement-learning extension. It does not require PyTorch, TensorFlow, or any third-party package. It uses a small online tabular Q-learning model inside the C simulator.

### State Space

Each scheduling state is bucketed from three runtime signals:

- ready-queue size: small, medium, or large;
- maximum waiting pressure: low, medium, or high;
- deadline slack: none, normal, or urgent.

This gives `3 * 3 * 3 = 27` states.

### Action Space

The learner chooses one scheduling expert at each decision point:

- `SRTF` expert: favors shortest remaining time.
- `Priority` expert: favors priority, aging, and urgent deadlines.
- `MLFQ` expert: uses classic MLFQ feedback queues.
- `RR` expert: favors time-sliced responsiveness.

### Online Training

Training is online. At each scheduling decision, the scheduler observes a state, selects an expert action with an epsilon-greedy policy, executes one simulated time unit, computes a reward, and updates the Q table:

```text
Q(s, a) <- Q(s, a) + alpha * (reward + gamma * max Q(s', *) - Q(s, a))
```

The reward encourages completed jobs and penalizes ready-queue pressure, long waiting time, urgent deadline pressure, context switches, and deadline misses.

## Build and Run

```bash
cd partA
make
make test
bash scripts/run_all.sh
```

No third-party Python package is required.

## Generated Results

- `results/raw.csv`: one summary row per algorithm and workload.
- `results/report.txt`: readable benchmark summary grouped by workload.
- `results/charts.md`: Markdown index for generated charts.
- `figures/avg_turnaround.svg`: average turnaround comparison.
- `figures/avg_response.svg`: average response-time comparison.
- `figures/deadline_misses.svg`: deadline-miss comparison.
- `figures/fairness.svg`: fairness comparison.
- `results/traces/*.csv`: Gantt-like execution segments.
- `results/processes/*.csv`: per-process metrics.

## Workloads

The benchmark currently includes nine workloads:

- `cpu_mixed.csv`
- `interactive_io.csv`
- `deadline_pressure.csv`
- `starvation_risk.csv`
- `short_burst_storm.csv`
- `io_heavy.csv`
- `long_tail_cpu.csv`
- `deadline_tight.csv`
- `mixed_large.csv`

## Experimental Summary

The current benchmark runs `8 algorithms * 9 workloads = 72` scheduling experiments.

Main observations:

- `aamlfq` improves classic `mlfq` on deadline and long-tail workloads. In `deadline_tight`, deadline misses drop from `5` with classic `mlfq` to `1` with `aamlfq`.
- In `cpu_mixed`, `aamlfq` reduces average turnaround from classic `mlfq`'s `32.833` to `26.833`, while also reducing deadline misses from `2` to `0`.
- In `long_tail_cpu`, `aamlfq` reduces average turnaround from `56.000` to `45.333` and deadline misses from `4` to `0`.
- Classic `mlfq` still provides very fast first response in some cases, for example `4.000` average response on `deadline_pressure` and `16.867` on `mixed_large`.
- `qlearn` remains strongest for response-time adaptation in many workloads, but it usually pays with more context switches.
- `srtf` and `sjf` still dominate average turnaround when burst times are known.

Therefore, the project now contains three scheduling layers: classical baselines, an explicitly improved MLFQ algorithm, and a reinforcement-learning scheduler.
