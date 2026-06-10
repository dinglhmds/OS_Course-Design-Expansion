#!/usr/bin/env python3
"""
Example: How to plug in a custom scheduling algorithm.

This file demonstrates the minimal boilerplate required to create a new
scheduler and evaluate it with the simulation engine.

Developers only need to:
  1. Inherit from BaseScheduler.
  2. Implement add_task() and next_task().
  3. (Optional) Override interrupt_handler() for time-quantum or completion hooks.
"""

from typing import Optional

from interfaces import BaseScheduler, Task


class ShortestJobFirstScheduler(BaseScheduler):
    """
    Custom Algorithm Demo: Non-Preemptive Shortest Job First (SJF)

    This is NOT one of the three baselines; it is here to show how easy
    it is to drop in a new policy.
    """

    def __init__(self):
        super().__init__("SJF-Custom")

    def add_task(self, task: Task) -> None:
        self.ready_queue.append(task)
        # Sort by burst time (shortest job first)
        self.ready_queue.sort(key=lambda t: t.burst_time)

    def next_task(self, current_time: int) -> Optional[Task]:
        # Non-preemptive: let the current task finish
        if self.current_task and self.current_task.remaining_time > 0:
            return self.current_task

        if self.ready_queue:
            self.current_task = self.ready_queue.pop(0)
            return self.current_task

        return None


# -----------------------------------------------------------------------------
# Developer template (copy-paste this to create your own algorithm):
# -----------------------------------------------------------------------------
# class MyAwesomeScheduler(BaseScheduler):
#     def __init__(self):
#         super().__init__("MyAwesome")
#
#     def add_task(self, task: Task) -> None:
#         # TODO: insert task into your custom data structure
#         pass
#
#     def next_task(self, current_time: int) -> Optional[Task]:
#         # TODO: implement your scheduling logic
#         # Return self.current_task to continue current execution,
#         # return a new Task to switch, or return None to idle.
#         pass
#
#     def interrupt_handler(self, task: Task, current_time: int) -> None:
#         # TODO: optional cleanup on task completion / quantum expiry
#         pass
# -----------------------------------------------------------------------------
