#!/usr/bin/env python3
"""
CPU Scheduling Simulator — Metrics & Analysis Tools
"""

import math
from typing import Dict, List

from interfaces import Task


def analyze_results(tasks: List[Task], total_time: int) -> Dict:
    """
    Compute batch-mode performance metrics from a list of completed tasks.
    """
    if not tasks:
        return {}

    n = len(tasks)
    turnaround_times = [t.turnaround_time for t in tasks]
    waiting_times = [t.waiting_time for t in tasks]
    weighted_turnaround = [t.turnaround_time / t.burst_time for t in tasks]

    avg_tat = sum(turnaround_times) / n
    avg_wt = sum(waiting_times) / n
    avg_wtat = sum(weighted_turnaround) / n

    if n > 0:
        mean_wt = avg_wt
        variance = sum((wt - mean_wt) ** 2 for wt in waiting_times) / n
        fairness_stddev = math.sqrt(variance)
    else:
        fairness_stddev = 0.0

    if sum(waiting_times) > 0:
        fairness_jain = (sum(waiting_times) ** 2) / (n * sum(wt ** 2 for wt in waiting_times))
    else:
        fairness_jain = 1.0

    throughput = n / total_time if total_time > 0 else 0.0

    return {
        "avg_turnaround_time": round(avg_tat, 2),
        "avg_waiting_time": round(avg_wt, 2),
        "avg_weighted_turnaround": round(avg_wtat, 2),
        "fairness_stddev": round(fairness_stddev, 2),
        "fairness_jain": round(fairness_jain, 4),
        "throughput": round(throughput, 4),
    }


def analyze_realtime_results(tasks: List[Task]) -> Dict:
    """
    Compute realtime-specific performance metrics.
    """
    if not tasks:
        return {}

    total_jobs = len(tasks)
    missed_jobs = sum(1 for t in tasks if t.missed_deadline)
    miss_rate = missed_jobs / total_jobs if total_jobs > 0 else 0.0

    response_times = [t.response_time for t in tasks if t.response_time is not None]
    if not response_times:
        return {
            "total_jobs": total_jobs,
            "missed_jobs": missed_jobs,
            "deadline_miss_rate": round(miss_rate, 4),
            "max_response_time": None,
            "avg_response_time": None,
            "jitter": None,
        }

    max_rt = max(response_times)
    avg_rt = sum(response_times) / len(response_times)
    jitter = math.sqrt(sum((rt - avg_rt) ** 2 for rt in response_times) / len(response_times)) if len(response_times) > 1 else 0.0

    return {
        "total_jobs": total_jobs,
        "missed_jobs": missed_jobs,
        "deadline_miss_rate": round(miss_rate, 4),
        "max_response_time": max_rt,
        "avg_response_time": round(avg_rt, 2),
        "jitter": round(jitter, 2),
    }


def analyze_concurrency(tasks: List[Task], total_time: int, num_cpus: int) -> Dict:
    """
    Compute multi-core concurrency metrics.

    Parameters
    ----------
    tasks : list[Task]
        All tasks that were part of the simulation.
    total_time : int
        Actual simulation duration.
    num_cpus : int
        Number of CPU cores used.

    Returns
    -------
    dict with:
        speedup          : sum(burst) / (total_time * num_cpus)
                           (≈ 1.0 when perfectly parallelised)
        migration_count  : total cross-core migrations across all tasks
        avg_migrations   : migration_count / num_tasks
    """
    if not tasks or total_time <= 0 or num_cpus <= 0:
        return {}

    total_burst = sum(t.burst_time for t in tasks)
    speedup = total_burst / (total_time * num_cpus)

    migration_count = sum(t.migration_count for t in tasks)
    avg_migrations = migration_count / len(tasks) if tasks else 0.0

    return {
        "speedup": round(speedup, 4),
        "migration_count": migration_count,
        "avg_migrations": round(avg_migrations, 2),
        "total_burst": total_burst,
        "total_time": total_time,
        "num_cpus": num_cpus,
    }
