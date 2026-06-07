/**
 * @file fcfs.c
 * @brief FCFS and SJF baseline schedulers (non-preemptive)
 */

#include "scheduler.h"
#include <float.h>

static void fcfs_init_priorities(TaskSet* ts) {
    for (int i = 0; i < ts->count; i++) {
        ts->tasks[i].priority = ts->tasks[i].arrival_time;
    }
}

static int fcfs_select_next(TaskSet* ts, double current_time) {
    for (int i = 0; i < ts->count; i++) {
        if (ts->tasks[i].state == TASK_RUNNING) {
            return i;
        }
    }

    int best_idx = -1;
    double earliest_arrival = DBL_MAX;

    for (int i = 0; i < ts->count; i++) {
        RealTimeTask* t = &ts->tasks[i];
        if (t->state == TASK_READY || t->state == TASK_PREEMPTED) {
            double orig_arrival = t->next_arrival - t->period;
            if (orig_arrival < earliest_arrival - EPSILON) {
                earliest_arrival = orig_arrival;
                best_idx = i;
            }
        }
    }
    return best_idx;
}

SchedulerOps g_fcfs_ops = {
    .name = "FCFS",
    .preemptive = false,
    .init_priorities = fcfs_init_priorities,
    .select_next = fcfs_select_next,
    .on_arrival = NULL,
    .on_complete = NULL,
    .on_miss = NULL
};

static void sjf_init_priorities(TaskSet* ts) {
    for (int i = 0; i < ts->count; i++) {
        ts->tasks[i].priority = ts->tasks[i].execution_time;
    }
}

static int sjf_select_next(TaskSet* ts, double current_time) {
    for (int i = 0; i < ts->count; i++) {
        if (ts->tasks[i].state == TASK_RUNNING) {
            return i;
        }
    }

    int best_idx = -1;
    double shortest_job = DBL_MAX;

    for (int i = 0; i < ts->count; i++) {
        RealTimeTask* t = &ts->tasks[i];
        if (t->state == TASK_READY || t->state == TASK_PREEMPTED) {
            if (t->execution_time < shortest_job - EPSILON) {
                shortest_job = t->execution_time;
                best_idx = i;
            }
        }
    }
    return best_idx;
}

SchedulerOps g_sjf_ops = {
    .name = "SJF",
    .preemptive = false,
    .init_priorities = sjf_init_priorities,
    .select_next = sjf_select_next,
    .on_arrival = NULL,
    .on_complete = NULL,
    .on_miss = NULL
};
