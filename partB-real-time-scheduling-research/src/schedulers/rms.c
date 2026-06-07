/**
 * @file rms.c
 * @brief RMS (Rate Monotonic Scheduling) implementation
 */

#include "scheduler.h"
#include "task.h"
#include <float.h>

static void rms_init_priorities(TaskSet* ts) {
    taskset_sort_by_period(ts);
    for (int i = 0; i < ts->count; i++) {
        ts->tasks[i].priority = (double)i;
    }
}

static int rms_select_next(TaskSet* ts, double current_time) {
    int best_idx = -1;
    double best_priority = DBL_MAX;

    for (int i = 0; i < ts->count; i++) {
        RealTimeTask* t = &ts->tasks[i];
        if (t->state == TASK_READY || t->state == TASK_PREEMPTED || t->state == TASK_RUNNING) {
            if (t->priority < best_priority) {
                best_priority = t->priority;
                best_idx = i;
            }
        }
    }
    return best_idx;
}

SchedulerOps g_rms_ops = {
    .name = "RMS",
    .preemptive = true,
    .init_priorities = rms_init_priorities,
    .select_next = rms_select_next,
    .on_arrival = NULL,
    .on_complete = NULL,
    .on_miss = NULL
};

bool rms_ll_test(const TaskSet* ts) {
    double u = taskset_total_utilization(ts);
    double bound = taskset_rms_bound(ts);
    return u <= bound + EPSILON;
}

bool rms_hyperbolic_test(const TaskSet* ts) {
    double product = 1.0;
    for (int i = 0; i < ts->count; i++) {
        product *= (task_utilization(&ts->tasks[i]) + 1.0);
        if (product > 2.0 + EPSILON) return false;
    }
    return product <= 2.0 + EPSILON;
}

bool rms_response_time_analysis(const TaskSet* ts, double* results) {
    if (ts->count == 0) return true;

    RealTimeTask sorted[MAX_TASKS];
    memcpy(sorted, ts->tasks, sizeof(RealTimeTask) * ts->count);

    for (int i = 0; i < ts->count - 1; i++) {
        for (int j = i + 1; j < ts->count; j++) {
            if (sorted[j].period < sorted[i].period) {
                RealTimeTask tmp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }
        }
    }

    bool all_schedulable = true;

    for (int i = 0; i < ts->count; i++) {
        double r = sorted[i].execution_time;
        int iter = 0;
        bool converged = false;

        while (iter < MAX_ITERATIONS) {
            double r_new = sorted[i].execution_time;
            for (int j = 0; j < i; j++) {
                r_new += ceil(r / sorted[j].period) * sorted[j].execution_time;
            }

            if (fabs(r_new - r) < EPSILON) {
                converged = true;
                break;
            }
            if (r_new > sorted[i].deadline + EPSILON) {
                converged = false;
                break;
            }
            r = r_new;
            iter++;
        }

        if (results) results[i] = r;

        if (!converged || r > sorted[i].deadline + EPSILON) {
            all_schedulable = false;
        }
    }

    return all_schedulable;
}
