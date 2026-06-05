#!/usr/bin/env python3
"""
Baseline 2: Round-Robin (RR) Scheduler
"""

from typing import Optional

from interfaces import BaseScheduler, Task


class RRScheduler(BaseScheduler):
    """Preemptive Round-Robin scheduler with a fixed time quantum."""

    def __init__(self, time_quantum: int = 4):
        super().__init__("RR")
        self.time_quantum = time_quantum
        self._quantum_used = 0  # how many ticks the current task has consumed

    def add_task(self, task: Task) -> None:
        self.ready_queue.append(task)

    def next_task(self, current_time: int) -> Optional[Task]:
        # If a task is running and still has work to do...
        if self.current_task and self.current_task.remaining_time > 0:
            self._quantum_used += 1
            if self._quantum_used < self.time_quantum:
                # Time slice not exhausted — keep running.
                return self.current_task
            else:
                # Quantum expired — preempt and push back to queue.
                self.ready_queue.append(self.current_task)
                self.current_task = None
                self._quantum_used = 0

        # Pick the next task from the head of the queue.
        if self.ready_queue:
            self.current_task = self.ready_queue.pop(0)
            self._quantum_used = 1  # will consume 1 tick now
            return self.current_task

        return None

    def interrupt_handler(self, task: Task, current_time: int) -> None:
        """Reset quantum counter when a task naturally finishes."""
        self._quantum_used = 0
        self.current_task = None
