#!/usr/bin/env python3
"""
Baseline 1: First-Come-First-Served (FCFS) Scheduler
"""

from typing import Optional

from interfaces import BaseScheduler, Task


class FCFSScheduler(BaseScheduler):
    """Non-preemptive FIFO scheduler."""

    def __init__(self):
        super().__init__("FCFS")

    def add_task(self, task: Task) -> None:
        self.ready_queue.append(task)

    def next_task(self, current_time: int) -> Optional[Task]:
        # If the current task is still running, let it continue.
        if self.current_task and self.current_task.remaining_time > 0:
            return self.current_task

        # Otherwise pick the next task from the FIFO queue.
        if self.ready_queue:
            self.current_task = self.ready_queue.pop(0)
            return self.current_task

        return None
