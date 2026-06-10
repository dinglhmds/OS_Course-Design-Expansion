#!/usr/bin/env python3
"""
Realtime Baseline: Rate Monotonic Scheduling (RMS)

Why RMS is suitable for realtime:
    RMS is a fixed-priority (static) scheduling algorithm.
    Priority is assigned *inversely* to the period: shorter period → higher priority.
    For a set of n periodic tasks on a uniprocessor, if the total CPU utilisation
    is below n*(2^(1/n)-1) (the Liu & Layland bound), RMS is guaranteed to meet
    all deadlines. This makes RMS predictable and widely used in hard-realtime
    embedded systems (e.g. automotive, avionics).

Multi-core compatibility:
    This implementation is a single-core baseline; it returns {0: task}
    so the engine places every chosen job on CPU 0.
"""

from typing import Dict, Optional

from interfaces import BaseScheduler, Task


class RMSScheduler(BaseScheduler):
    """Rate Monotonic Scheduling (RMS) scheduler — single-core baseline."""

    def __init__(self):
        super().__init__("RMS")

    def add_task(self, task: Task) -> None:
        self.ready_queue.append(task)
        self.ready_queue.sort(key=lambda t: t.period)

    def schedule(
        self,
        current_time: int,
        cpu_status_dict: Dict[int, Optional[Task]],
    ) -> Dict[int, Optional[Task]]:
        self.ready_queue = [t for t in self.ready_queue if t.remaining_time > 0]

        if not self.ready_queue:
            self.current_task = None
            return {0: None}

        self.ready_queue.sort(key=lambda t: t.period)
        self.current_task = self.ready_queue[0]
        return {0: self.current_task}
