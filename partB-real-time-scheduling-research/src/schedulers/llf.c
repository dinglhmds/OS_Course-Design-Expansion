/**
 * @file llf.c
 * @brief LLF (Least Laxity First) implementation
 */

#include "scheduler.h"
#include <float.h>

static void llf_init_priorities(TaskSet* ts) {
    for (int i = 0; i < ts->count; i++) {
        ts->tasks[i].priority = 0.0;
    }
}

static int llf_select_next(TaskSet* ts, double current_time) {
    int best_idx = -1;
    double min_laxity = DBL_MAX;

    for (int i = 0; i < ts->count; i++) {
        RealTimeTask* t = &ts->tasks[i];
        if (t->state == TASK_READY || t->state == TASK_PREEMPTED || t->state == TASK_RUNNING) {
            double laxity = task_slack(t, current_time);
            if (laxity < min_laxity - EPSILON) {
                min_laxity = laxity;
                best_idx = i;
            }
        }
    }
    return best_idx;
}

SchedulerOps g_llf_ops = {
    .name = "LLF",
    .preemptive = true,
    .init_priorities = llf_init_priorities,
    .select_next = llf_select_next,
    .on_arrival = NULL,
    .on_complete = NULL,
    .on_miss = NULL
};
