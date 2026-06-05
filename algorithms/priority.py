#!/usr/bin/env python3
"""
Baseline 3: Non-Preemptive Priority Scheduler
(lower priority number = higher priority)
"""

from typing import Optional

from interfaces import BaseScheduler, Task


class PriorityScheduler(BaseScheduler):
    """Non-preemptive priority scheduler."""

    def __init__(self):
        super().__init__("Priority")

    def add_task(self, task: Task) -> None:
        self.ready_queue.append(task)
        # Keep queue sorted by priority (ascending = higher priority first)
        self.ready_queue.sort(key=lambda t: t.priority)

    def next_task(self, current_time: int) -> Optional[Task]:
        # Non-preemptive: once a task starts, it runs to completion.
        if self.current_task and self.current_task.remaining_time > 0:
            return self.current_task

        if self.ready_queue:
            self.current_task = self.ready_queue.pop(0)
            return self.current_task

        return None
