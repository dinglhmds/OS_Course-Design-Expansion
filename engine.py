#!/usr/bin/env python3
"""
CPU Scheduling Simulator — Discrete-Event Simulation Engine

Supports three execution models:
  1. Batch single-core    — legacy mode, num_cpus=1.
  2. Batch multi-core     — global queue + multi-core assignment.
  3. Realtime multi-core  — periodic tasks on multiple CPUs.
"""

from typing import Dict, List, Optional, Tuple

from interfaces import BaseScheduler, Task


# =============================================================================
# Batch Mode (now supports multi-core)
# =============================================================================
def run_simulation(
    scheduler: BaseScheduler,
    task_list: List[Task],
    num_cpus: int = 1,
) -> Tuple[List[Task], List[Tuple], int]:
    """
    Run a discrete-time batch simulation.

    Parameters
    ----------
    scheduler : BaseScheduler
        The scheduling algorithm to evaluate.
    task_list : list[Task]
        All tasks that will arrive during the simulation.
    num_cpus : int
        Number of CPU cores (default 1 for backward compatibility).

    Returns
    -------
    completed_tasks : list[Task]
    gantt : list[tuple]
        Format: (pid_or_none, start, end, cpu_id)
    total_time : int
    """
    scheduler.reset()
    tasks = sorted(task_list, key=lambda t: t.arrival_time)

    current_time = 0
    task_idx = 0
    num_tasks = len(tasks)
    completed_tasks: List[Task] = []
    gantt: List[Tuple] = []

    # Per-CPU running state
    cpu_running: Dict[int, Optional[Task]] = {i: None for i in range(num_cpus)}

    while len(completed_tasks) < num_tasks:
        # 1. Admit newly arrived tasks
        while task_idx < num_tasks and tasks[task_idx].arrival_time <= current_time:
            scheduler.add_task(tasks[task_idx])
            task_idx += 1

        # 2. Build cpu_status_dict and ask scheduler for assignments
        cpu_status_dict = {cpu_id: task for cpu_id, task in cpu_running.items()}
        assignments = scheduler.schedule(current_time, cpu_status_dict)

        # 3. Apply assignments and detect migrations
        for cpu_id in range(num_cpus):
            assigned = assignments.get(cpu_id)
            currently_running = cpu_running[cpu_id]

            if assigned is not None:
                # Migration detection:
                # Task was previously running on a *different* core.
                if (assigned != currently_running
                        and assigned.last_cpu != -1
                        and assigned.last_cpu != cpu_id):
                    old_cpu = assigned.last_cpu
                    if cpu_running[old_cpu] == assigned:
                        cpu_running[old_cpu] = None
                    assigned.remaining_time += 1  # cache-miss penalty
                    assigned.migration_count += 1
                    print(
                        f"[DEBUG] Migration: Task {assigned.pid} "
                        f"CPU {old_cpu} -> {cpu_id} @ t={current_time}"
                    )

                cpu_running[cpu_id] = assigned
                assigned.last_cpu = cpu_id
            else:
                cpu_running[cpu_id] = None

        # 4. Execute on all CPUs
        for cpu_id in range(num_cpus):
            assigned = cpu_running[cpu_id]
            if assigned is not None:
                if assigned.start_time is None:
                    assigned.start_time = current_time

                gantt.append((assigned.pid, current_time, current_time + 1, cpu_id))

                assigned.remaining_time -= 1

                # Update waiting time for tasks in the ready queue
                for t in scheduler.ready_queue:
                    if t != assigned and t.remaining_time > 0:
                        t.waiting_time += 1

                # Check for completion
                if assigned.remaining_time <= 0:
                    assigned.completion_time = current_time + 1
                    assigned.turnaround_time = assigned.completion_time - assigned.arrival_time
                    completed_tasks.append(assigned)
                    scheduler.interrupt_handler(assigned, current_time + 1)
                    cpu_running[cpu_id] = None
            else:
                gantt.append((None, current_time, current_time + 1, cpu_id))

        current_time += 1

    return completed_tasks, gantt, current_time


# =============================================================================
# Realtime Mode (now supports multi-core)
# =============================================================================
def run_realtime_simulation(
    scheduler: BaseScheduler,
    task_configs: List[Dict],
    duration: int,
    num_cpus: int = 1,
) -> Tuple[List[Task], List[Tuple], int]:
    """
    Run a periodic realtime simulation.

    Parameters
    ----------
    scheduler : BaseScheduler
    task_configs : list[dict]
        Periodic task descriptions.
    duration : int
    num_cpus : int
        Number of CPU cores (default 1).

    Returns
    -------
    all_jobs : list[Task]
    gantt : list[tuple]
        Format: (pid_or_none, start, end, missed_bool, cpu_id)
    total_time : int
    """
    scheduler.reset()

    current_time = 0
    job_counter = 0
    all_jobs: List[Task] = []
    gantt: List[Tuple] = []

    cpu_running: Dict[int, Optional[Task]] = {i: None for i in range(num_cpus)}

    while current_time < duration:
        # 1. Generate periodic jobs
        for cfg in task_configs:
            if current_time % cfg["period"] == 0:
                job_counter += 1
                task = Task(
                    pid=job_counter,
                    arrival_time=current_time,
                    burst_time=cfg["wcet"],
                    priority=0,
                    period=cfg["period"],
                    deadline=cfg["deadline"],
                    wcet=cfg["wcet"],
                    absolute_deadline=current_time + cfg["deadline"],
                )
                scheduler.add_task(task)
                all_jobs.append(task)

        # 2. Schedule across all cores
        cpu_status_dict = {cpu_id: task for cpu_id, task in cpu_running.items()}
        assignments = scheduler.schedule(current_time, cpu_status_dict)

        # 3. Apply assignments and detect migrations
        for cpu_id in range(num_cpus):
            assigned = assignments.get(cpu_id)
            currently_running = cpu_running[cpu_id]

            if assigned is not None:
                if (assigned != currently_running
                        and assigned.last_cpu != -1
                        and assigned.last_cpu != cpu_id):
                    old_cpu = assigned.last_cpu
                    if cpu_running[old_cpu] == assigned:
                        cpu_running[old_cpu] = None
                    assigned.remaining_time += 1
                    assigned.migration_count += 1
                    print(
                        f"[DEBUG] Migration: Task {assigned.pid} "
                        f"CPU {old_cpu} -> {cpu_id} @ t={current_time}"
                    )

                cpu_running[cpu_id] = assigned
                assigned.last_cpu = cpu_id
            else:
                cpu_running[cpu_id] = None

        # 4. Execute on all cores
        for cpu_id in range(num_cpus):
            assigned = cpu_running[cpu_id]
            if assigned is not None:
                if assigned.start_time is None:
                    assigned.start_time = current_time

                is_missed = False
                gantt.append((assigned.pid, current_time, current_time + 1, is_missed, cpu_id))

                assigned.remaining_time -= 1

                if assigned.remaining_time <= 0:
                    assigned.completion_time = current_time + 1
                    assigned.turnaround_time = assigned.completion_time - assigned.arrival_time
                    assigned.response_time = assigned.completion_time - assigned.arrival_time

                    if assigned.completion_time > assigned.absolute_deadline:
                        assigned.missed_deadline = True
                        if gantt and gantt[-1][0] == assigned.pid:
                            gantt[-1] = (assigned.pid, gantt[-1][1], current_time + 1, True, cpu_id)

                    scheduler.interrupt_handler(assigned, current_time + 1)
                    cpu_running[cpu_id] = None
            else:
                gantt.append((None, current_time, current_time + 1, False, cpu_id))

        current_time += 1

    # Post-simulation: mark unfinished jobs that missed their deadline
    for t in all_jobs:
        if t.completion_time is None and current_time > t.absolute_deadline:
            t.missed_deadline = True

    return all_jobs, gantt, current_time
