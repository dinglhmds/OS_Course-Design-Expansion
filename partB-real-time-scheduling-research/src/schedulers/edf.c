/**
 * @file edf.c
 * @brief EDF (Earliest Deadline First) implementation
 */

#include "scheduler.h"
#include <float.h>

static void edf_init_priorities(TaskSet* ts) {
    for (int i = 0; i < ts->count; i++) {
        ts->tasks[i].priority = 0.0;
    }
}

static int edf_select_next(TaskSet* ts, double current_time) {
    int best_idx = -1;
    double earliest_deadline = DBL_MAX;

    for (int i = 0; i < ts->count; i++) {
        RealTimeTask* t = &ts->tasks[i];
        if (t->state == TASK_READY || t->state == TASK_PREEMPTED || t->state == TASK_RUNNING) {
            if (t->absolute_deadline < earliest_deadline - EPSILON) {
                earliest_deadline = t->absolute_deadline;
                best_idx = i;
            }
        }
    }
    return best_idx;
}

SchedulerOps g_edf_ops = {
    .name = "EDF",
    .preemptive = true,
    .init_priorities = edf_init_priorities,
    .select_next = edf_select_next,
    .on_arrival = NULL,
    .on_complete = NULL,
    .on_miss = NULL
};

bool edf_utilization_test(const TaskSet* ts) {
    double u = taskset_total_utilization(ts);
    return u <= 1.0 + EPSILON;
}

bool edf_density_test(const TaskSet* ts) {
    double density = 0.0;
    for (int i = 0; i < ts->count; i++) {
        if (ts->tasks[i].deadline > EPSILON) {
            density += ts->tasks[i].execution_time / ts->tasks[i].deadline;
        }
    }
    return density <= 1.0 + EPSILON;
}

bool edf_processor_demand_test(const TaskSet* ts) {
    double hp = 1.0;
    for (int i = 0; i < ts->count; i++) {
        hp = fmax(hp, ts->tasks[i].period);
    }
    if (hp > 10000.0) hp = 10000.0;

    double points[MAX_EVENTS];
    int pcount = 0;

    for (int i = 0; i < ts->count; i++) {
        double t = ts->tasks[i].deadline;
        while (t <= hp + EPSILON && pcount < MAX_EVENTS) {
            points[pcount++] = t;
            t += ts->tasks[i].period;
        }
    }

    for (int i = 0; i < pcount - 1; i++) {
        for (int j = i + 1; j < pcount; j++) {
            if (points[j] < points[i]) {
                double tmp = points[i];
                points[i] = points[j];
                points[j] = tmp;
            }
        }
    }

    for (int k = 0; k < pcount; k++) {
        if (k > 0 && fabs(points[k] - points[k-1]) < EPSILON) continue;

        double L = points[k];
        double demand = 0.0;
        for (int i = 0; i < ts->count; i++) {
            if (L >= ts->tasks[i].deadline - EPSILON) {
                double jobs = floor((L - ts->tasks[i].deadline) / ts->tasks[i].period) + 1.0;
                if (jobs < 0) jobs = 0;
                demand += jobs * ts->tasks[i].execution_time;
            }
        }
        if (demand > L + EPSILON) return false;
    }

    return true;
}
