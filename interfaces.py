#!/usr/bin/env python3
"""
CPU Scheduling Simulator — Interface Definitions

Supports three execution models:
  1. Batch single-core    — legacy next_task() API.
  2. Realtime single-core — periodic tasks with schedule() returning {0: task}.
  3. Multi-core           — schedule() returns {cpu_id: task, ...} for all cores.
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from typing import Dict, Optional


@dataclass
class Task:
    """
    Represents a single CPU-bound task or job.

    Attributes:
        pid, arrival_time, burst_time, priority, remaining_time
        period, deadline, wcet, absolute_deadline, missed_deadline, response_time
        last_cpu        : CPU core this task last ran on (-1 = never ran).
        migration_count : Number of times this task migrated between cores.
    """
    pid: int
    arrival_time: int
    burst_time: int
    priority: int = 0
    remaining_time: int = field(init=False)

    # --- Realtime extensions ---
    period: Optional[int] = None
    deadline: Optional[int] = None
    wcet: Optional[int] = None
    absolute_deadline: Optional[int] = None
    missed_deadline: bool = False
    response_time: Optional[int] = None

    # --- Multi-core extensions ---
    last_cpu: int = -1          # Last CPU core this task ran on
    migration_count: int = 0    # Number of cross-core migrations

    # --- Runtime statistics ---
    start_time: Optional[int] = None
    completion_time: Optional[int] = None
    turnaround_time: Optional[int] = None
    waiting_time: int = 0

    def __post_init__(self):
        self.remaining_time = self.burst_time
        if self.deadline is not None and self.absolute_deadline is None:
            self.absolute_deadline = self.arrival_time + self.deadline
        if self.wcet is None:
            self.wcet = self.burst_time

    def __repr__(self):
        return (f"Task(pid={self.pid}, arrival={self.arrival_time}, "
                f"burst={self.burst_time}, prio={self.priority}, "
                f"remaining={self.remaining_time}, "
                f"cpu={self.last_cpu})")


class BaseScheduler(ABC):
    """
    Abstract base class for all scheduling algorithms.

    Core method — schedule(current_time, cpu_status_dict) -> dict:
        Returns a mapping {cpu_id: task_or_None} telling the engine which
        task each core should execute during the current tick.

    Backward compatibility:
        Single-core algorithms (FCFS, RR, Priority, EDF, RMS) may ignore the
        multi-core dictionary and simply return {0: task}.  The engine handles
        both styles transparently.
    """

    def __init__(self, name: str = "BaseScheduler"):
        self.name = name
        self.ready_queue: list[Task] = []
        self.current_task: Optional[Task] = None

    @abstractmethod
    def add_task(self, task: Task) -> None:
        """Insert a newly arrived task into the scheduler's data structure."""
        ...

    def next_task(self, current_time: int) -> Optional[Task]:
        """
        Legacy single-core scheduling hook.
        Non-realtime single-core schedulers should override this.
        """
        raise NotImplementedError(
            f"{self.name} does not implement next_task()."
        )

    def schedule(
        self,
        current_time: int,
        cpu_status_dict: Dict[int, Optional[Task]],
    ) -> Dict[int, Optional[Task]]:
        """
        Core multi-core scheduling decision.

        Parameters
        ----------
        current_time : int
            Current simulation clock tick.
        cpu_status_dict : dict[int, Task|None]
            Snapshot of what each CPU core is currently running.
            Format: {0: task_or_None, 1: task_or_None, ...}

        Returns
        -------
        dict[int, Task|None]
            Assignment for the upcoming tick.
            {cpu_id: task_to_run} — run this task on this core.
            {cpu_id: None}        — leave this core idle.

        Default implementation:
            Delegates to next_task() and wraps the result for CPU 0.
            This allows legacy single-core schedulers to work unchanged.
        """
        task = self.next_task(current_time)
        return {0: task}

    def interrupt_handler(self, task: Task, current_time: int) -> None:
        """Hook invoked when a task finishes or its quantum expires."""
        pass

    def reset(self) -> None:
        """Reset internal state for a fresh simulation run."""
        self.ready_queue.clear()
        self.current_task = None
