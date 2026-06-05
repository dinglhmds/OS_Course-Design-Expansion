#!/usr/bin/env python3
"""
CPU Scheduling Simulator — Main Entry Point

Supports three modes:
  batch    : classic non-realtime comparison (FCFS, RR, Priority, SJF)
  realtime : periodic realtime task comparison (FCFS, RR, EDF, RMS)
  multicore: multi-core Global Round-Robin with migration penalty
"""

import argparse
import random
from typing import List

from engine import run_simulation, run_realtime_simulation
from interfaces import Task
from metrics import analyze_results, analyze_realtime_results, analyze_concurrency
from report_generator import plot_gantt, save_csv

# Import algorithms
from algorithms.fcfs import FCFSScheduler
from algorithms.rr import RRScheduler
from algorithms.priority import PriorityScheduler
from algorithms.custom_demo import ShortestJobFirstScheduler
from algorithms.edf import EDFScheduler
from algorithms.rms import RMSScheduler
from algorithms.multi_core import GlobalRoundRobin


# =============================================================================
# Batch Mode Helpers
# =============================================================================
def generate_tasks(
    count: int = 8,
    seed: int = 42,
    max_arrival: int = 10,
    max_burst: int = 10,
) -> List[Task]:
    """Generate a reproducible random task stream."""
    rng = random.Random(seed)
    tasks = []
    for i in range(count):
        tasks.append(Task(
            pid=i + 1,
            arrival_time=rng.randint(0, max_arrival),
            burst_time=rng.randint(1, max_burst),
            priority=rng.randint(1, 10),
        ))
    return tasks


def print_batch_table(results: dict) -> None:
    header = (
        f"{'Algorithm':<15} {'Avg_TAT':>10} {'Avg_WT':>10} "
        f"{'Avg_WTAT':>10} {'Fair_Std':>10} {'Fair_Jain':>10} {'Thrpt':>8}"
    )
    print("\n" + "=" * len(header))
    print(header)
    print("=" * len(header))
    for name, m in results.items():
        print(
            f"{name:<15} "
            f"{m.get('avg_turnaround_time', 0):>10.2f} "
            f"{m.get('avg_waiting_time', 0):>10.2f} "
            f"{m.get('avg_weighted_turnaround', 0):>10.2f} "
            f"{m.get('fairness_stddev', 0):>10.2f} "
            f"{m.get('fairness_jain', 0):>10.4f} "
            f"{m.get('throughput', 0):>8.4f}"
        )
    print("=" * len(header))


def run_batch_mode(args) -> None:
    """Run the classic batch-mode benchmark."""
    task_stream = generate_tasks(count=args.tasks, seed=args.seed)
    print(f"Generated {len(task_stream)} tasks (seed={args.seed}):")
    print(f"{'PID':>4} {'Arrival':>8} {'Burst':>6} {'Priority':>9}")
    print("-" * 32)
    for t in task_stream:
        print(f"{t.pid:>4} {t.arrival_time:>8} {t.burst_time:>6} {t.priority:>9}")

    algorithms = [
        FCFSScheduler(),
        RRScheduler(time_quantum=args.quantum),
        PriorityScheduler(),
        ShortestJobFirstScheduler(),
    ]

    all_metrics = {}
    all_gantt = {}

    for scheduler in algorithms:
        fresh_tasks = [
            Task(pid=t.pid, arrival_time=t.arrival_time,
                 burst_time=t.burst_time, priority=t.priority)
            for t in task_stream
        ]
        completed, gantt, total_time = run_simulation(scheduler, fresh_tasks)
        metrics = analyze_results(completed, total_time)
        all_metrics[scheduler.name] = metrics
        all_gantt[scheduler.name] = gantt
        print(f"\n[{scheduler.name}] simulation finished in {total_time} time units.")

    print_batch_table(all_metrics)
    save_csv(all_metrics, filename="report.csv")
    if not args.no_chart:
        plot_gantt(all_gantt, filename="gantt_chart.png", title="Batch Scheduling Comparison")


# =============================================================================
# Realtime Mode Helpers
# =============================================================================
def print_realtime_table(results: dict) -> None:
    header = (
        f"{'Algorithm':<15} {'Jobs':>6} {'Missed':>7} "
        f"{'Miss_Rate':>10} {'Max_RT':>8} {'Avg_RT':>8} {'Jitter':>8}"
    )
    print("\n" + "=" * len(header))
    print(header)
    print("=" * len(header))
    for name, m in results.items():
        print(
            f"{name:<15} "
            f"{m.get('total_jobs', 0):>6} "
            f"{m.get('missed_jobs', 0):>7} "
            f"{m.get('deadline_miss_rate', 0):>10.4f} "
            f"{m.get('max_response_time', 0) or 0:>8} "
            f"{m.get('avg_response_time', 0) or 0:>8.2f} "
            f"{m.get('jitter', 0) or 0:>8.2f}"
        )
    print("=" * len(header))


def run_realtime_mode(args) -> None:
    """
    Realtime Stress Test.
    """
    task_configs = [
        {"name": "T1", "period": 5,  "wcet": 2, "deadline": 5},
        {"name": "T2", "period": 10, "wcet": 3, "deadline": 10},
        {"name": "T3", "period": 15, "wcet": 3, "deadline": 15},
    ]
    duration = args.duration

    print("Realtime Stress Test")
    print("-" * 40)
    print(f"{'Task':>6} {'Period':>8} {'WCET':>6} {'Deadline':>9}")
    for cfg in task_configs:
        print(f"{cfg['name']:>6} {cfg['period']:>8} {cfg['wcet']:>6} {cfg['deadline']:>9}")
    print(f"\nSimulation duration: {duration} time units")

    algorithms = [
        FCFSScheduler(),
        RRScheduler(time_quantum=args.quantum),
        EDFScheduler(),
        RMSScheduler(),
    ]

    all_metrics = {}
    all_gantt = {}

    for scheduler in algorithms:
        jobs, gantt, total_time = run_realtime_simulation(
            scheduler, task_configs, duration
        )
        metrics = analyze_realtime_results(jobs)
        all_metrics[scheduler.name] = metrics
        all_gantt[scheduler.name] = gantt
        print(
            f"[{scheduler.name:>12}] finished: {len(jobs)} jobs released, "
            f"{metrics['missed_jobs']} missed."
        )

    print_realtime_table(all_metrics)
    save_csv(all_metrics, filename="report_realtime.csv")
    if not args.no_chart:
        plot_gantt(
            all_gantt,
            filename="gantt_realtime.png",
            title="Realtime Scheduling Comparison (Red = Missed Deadline)",
        )


# =============================================================================
# Multi-Core Mode Helpers
# =============================================================================
def print_multicore_table(results: dict) -> None:
    header = (
        f"{'Algorithm':<15} {'Speedup':>10} {'Migrations':>12} "
        f"{'Avg_Mig':>10} {'Total_Time':>12}"
    )
    print("\n" + "=" * len(header))
    print(header)
    print("=" * len(header))
    for name, m in results.items():
        print(
            f"{name:<15} "
            f"{m.get('speedup', 0):>10.4f} "
            f"{m.get('migration_count', 0):>12} "
            f"{m.get('avg_migrations', 0):>10.2f} "
            f"{m.get('total_time', 0):>12}"
        )
    print("=" * len(header))


def run_multicore_mode(args) -> None:
    """
    Multi-Core Test.

    Setup: 2 CPUs, 4 tasks, burst=10 each.
    Expected: total_time ≈ 20, speedup ≈ 1.0 (perfect parallelism).
    """
    num_cpus = args.num_cpus
    task_stream = [
        Task(pid=1, arrival_time=0, burst_time=10),
        Task(pid=2, arrival_time=0, burst_time=10),
        Task(pid=3, arrival_time=0, burst_time=10),
        Task(pid=4, arrival_time=0, burst_time=10),
    ]

    print(f"Multi-Core Test: {num_cpus} CPUs, {len(task_stream)} tasks")
    print(f"Each task burst=10, total work={sum(t.burst_time for t in task_stream)}")
    print(f"Theoretical minimum time (perfect parallel): {sum(t.burst_time for t in task_stream) / num_cpus}\n")

    scheduler = GlobalRoundRobin(time_quantum=args.quantum)
    completed, gantt, total_time = run_simulation(
        scheduler, task_stream, num_cpus=num_cpus
    )

    metrics = analyze_concurrency(completed, total_time, num_cpus)
    metrics.update(analyze_results(completed, total_time))

    print(f"Actual total time: {total_time}")
    print(f"Speedup: {metrics['speedup']:.4f}")
    print(f"Migration count: {metrics['migration_count']}")
    print(f"Avg migrations per task: {metrics['avg_migrations']:.2f}")

    print_multicore_table({scheduler.name: metrics})
    save_csv({scheduler.name: metrics}, filename="report_multicore.csv")
    if not args.no_chart:
        plot_gantt(
            {scheduler.name: gantt},
            filename="gantt_multicore.png",
            title=f"Multi-Core Scheduling ({num_cpus} CPUs)",
        )


# =============================================================================
# Entry Point
# =============================================================================
def main():
    parser = argparse.ArgumentParser(
        description="CPU Scheduling Simulator Framework (Batch + Realtime + Multi-Core)",
    )
    parser.add_argument(
        "--mode",
        choices=["batch", "realtime", "multicore"],
        default="batch",
        help="Simulation mode (default: batch)",
    )
    parser.add_argument(
        "--tasks", type=int, default=8,
        help="Number of tasks to generate in batch mode (default: 8)",
    )
    parser.add_argument(
        "--seed", type=int, default=42,
        help="Random seed for reproducibility (default: 42)",
    )
    parser.add_argument(
        "--quantum", type=int, default=4,
        help="Time quantum for RR (default: 4)",
    )
    parser.add_argument(
        "--duration", type=int, default=60,
        help="Simulation duration for realtime mode (default: 60)",
    )
    parser.add_argument(
        "--num-cpus", type=int, default=2,
        help="Number of CPU cores for multicore mode (default: 2)",
    )
    parser.add_argument(
        "--no-chart", action="store_true",
        help="Skip matplotlib chart generation",
    )
    args = parser.parse_args()

    if args.mode == "batch":
        run_batch_mode(args)
    elif args.mode == "realtime":
        run_realtime_mode(args)
    else:
        run_multicore_mode(args)


if __name__ == "__main__":
    main()
