# Part D: Concurrency Performance Optimization

**SPECIAL NOTE**: This code is ONLY intended to be used as the EXPANSION part in the OS(Operating System) coursework/project during 2026 Spring semester of SCUT for demonstration.

This module provides a small, reproducible operating systems coursework/lab for comparing correctness and performance of shared-counter implementations under concurrent updates.

The benchmark is designed for Linux or WSL and uses C11, pthreads, and Python plotting. It keeps the total number of counter operations fixed, then varies the thread count to show how synchronization and shared writes affect scalability.

## Implemented Modes

- `race`: all threads directly update a shared counter with `counter++`. This intentionally contains a data race and may produce incorrect results.
- `mutex`: protects the shared counter with `pthread_mutex_t`.
- `spin`: protects the shared counter with `pthread_spinlock_t`.
- `atomic`: uses C11 `_Atomic long long` and `atomic_fetch_add_explicit(..., memory_order_relaxed)`.
- `local`: each thread accumulates locally, and the main thread sums the partial counters after all workers finish.

The `race` mode is included to demonstrate incorrect concurrency. The other modes are intended for correctness and performance comparison.

## Build and Run

```bash
cd Part_D-Concurrency_Performance_Optimization
make
bash scripts/run_all.sh # bash scripts/run_all.sh 2>&1 | tee -a output.log
python3 scripts/plot.py
```

The batch script uses these defaults:

- `TOTAL_ITERS=10000000`
- `REPEAT=5`
- modes: `race mutex spin atomic local`
- threads: `1 2 4 8 16`

You can override the defaults when running the script:

```bash
TOTAL_ITERS=1000000 REPEAT=3 bash scripts/run_all.sh
```

## Direct Benchmark Usage

```bash
./bench_counter --mode atomic --threads 4 --iters 10000000 --repeat 5
```

The program prints CSV rows with these fields:

- `mode`: benchmark mode.
- `threads`: number of worker threads.
- `total_iters`: total operations across all threads.
- `repeat`: repeat index inside one benchmark invocation.
- `elapsed_sec`: elapsed wall-clock time measured with `CLOCK_MONOTONIC`.
- `ns_per_op`: average nanoseconds per counter operation.
- `throughput`: operations per second.
- `final_counter`: final counter value observed by the benchmark.
- `correct`: `1` if `final_counter == total_iters`, otherwise `0`.

## Output Files

- `results/raw.csv`: raw CSV rows from all benchmark runs.
- `results/summary.csv`: grouped mean and standard deviation for `ns_per_op` and `throughput`.
- `figures/ns_per_op.png`: latency comparison across modes and thread counts.
- `figures/throughput.png`: throughput comparison across modes and thread counts.
