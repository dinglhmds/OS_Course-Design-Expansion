#!/usr/bin/env python3
"""
Realtime Baseline: Earliest Deadline First (EDF)

Why EDF is suitable for realtime:
    EDF is an optimal dynamic-priority algorithm for uniprocessor systems.
    At any point it runs the ready job with the *earliest absolute deadline*.
    If a feasible schedule exists, EDF will find it (assuming preemption is
    allowed and tasks are independent). This makes it the gold standard for
    soft-realtime and hard-realtime systems where deadlines are known.

Multi-core compatibility:
    This implementation is a single-core baseline; it returns {0: task}
    so the engine places every chosen job on CPU 0.
"""

from typing import Dict, Optional

from interfaces import BaseScheduler, Task


class EDFScheduler(BaseScheduler):
    """Earliest Deadline First (EDF) scheduler — single-core baseline."""

    def __init__(self):
        super().__init__("EDF")

    def add_task(self, task: Task) -> None:
        self.ready_queue.append(task)
        self.ready_queue.sort(key=lambda t: t.absolute_deadline)

    def schedule(
        self,
        current_time: int,
        cpu_status_dict: Dict[int, Optional[Task]],
    ) -> Dict[int, Optional[Task]]:
        self.ready_queue = [t for t in self.ready_queue if t.remaining_time > 0]

        if not self.ready_queue:
            self.current_task = None
            return {0: None}

        self.ready_queue.sort(key=lambda t: t.absolute_deadline)
        self.current_task = self.ready_queue[0]
        return {0: self.current_task}
