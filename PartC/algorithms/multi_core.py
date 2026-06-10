#!/usr/bin/env python3
"""
Multi-Core Baseline: Global Round-Robin Scheduler

This is a simple multi-core scheduling strategy that demonstrates the
"global queue + multi-core allocation" model.

Logic:
    - Maintain a single global ready queue shared by all CPU cores.
    - At each tick, iterate over all cores.
    - If a core is idle, pull the next task from the global queue.
    - If a core's current task exhausted its quantum, preempt it and
      assign a new task from the queue.

Why this is useful:
    Global scheduling naturally load-balances across cores because tasks
    are not pinned to a specific core.  The trade-off is higher migration
    cost (cache affinity loss), which the engine simulates via the
    migration-penalty mechanism.
"""

from typing import Dict, Optional

from interfaces import BaseScheduler, Task


class GlobalRoundRobin(BaseScheduler):
    """
    Global Round-Robin scheduler for multi-core systems.

    A single ready queue is shared among all cores.
    Each core has its own time-quantum counter.
    """

    def __init__(self, time_quantum: int = 4):
        super().__init__("GlobalRR")
        self.time_quantum = time_quantum
        self._cpu_quantum: Dict[int, int] = {}  # cpu_id -> consecutive ticks consumed

    def add_task(self, task: Task) -> None:
        self.ready_queue.append(task)

    def schedule(
        self,
        current_time: int,
        cpu_status_dict: Dict[int, Optional[Task]],
    ) -> Dict[int, Optional[Task]]:
        assignments: Dict[int, Optional[Task]] = {}
        already_assigned: set[int] = set()  # object ids to prevent double assignment

        for cpu_id, running_task in sorted(cpu_status_dict.items()):
            # If the core has a running task that still has work...
            if running_task is not None and running_task.remaining_time > 0:
                q = self._cpu_quantum.get(cpu_id, 0) + 1
                if q < self.time_quantum:
                    assignments[cpu_id] = running_task
                    self._cpu_quantum[cpu_id] = q
                    already_assigned.add(id(running_task))
                    continue
                else:
                    # Quantum expired — preempt and return to global queue
                    self.ready_queue.append(running_task)
                    self._cpu_quantum[cpu_id] = 0

            # Find a new task from the global queue
            chosen: Optional[Task] = None
            for i, task in enumerate(self.ready_queue):
                if task.remaining_time > 0 and id(task) not in already_assigned:
                    chosen = task
                    self.ready_queue.pop(i)
                    break

            if chosen is not None:
                assignments[cpu_id] = chosen
                self._cpu_quantum[cpu_id] = 1
                already_assigned.add(id(chosen))
            else:
                assignments[cpu_id] = None
                self._cpu_quantum[cpu_id] = 0

        return assignments
