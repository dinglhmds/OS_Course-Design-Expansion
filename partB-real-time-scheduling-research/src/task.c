/**
 * @file task.c
 * @brief Real-time task management implementation
 */

#include "task.h"
#include <float.h>

void task_init(RealTimeTask* task, int id, double period, double wcet,
               double deadline, double arrival, const char* name) {
    assert(task != NULL);
    assert(period > 0);
    assert(wcet > 0);

    task->task_id = id;
    task->period = period;
    task->execution_time = wcet;
    task->deadline = (deadline > 0) ? deadline : period;
    task->arrival_time = arrival;
    task->priority = 0.0;

    if (name && strlen(name) > 0) {
        strncpy(task->name, name, MAX_NAME_LEN - 1);
        task->name[MAX_NAME_LEN - 1] = '\0';
    } else {
        snprintf(task->name, MAX_NAME_LEN, "Task-%d", id);
    }

    task_reset(task);
}

void task_reset(RealTimeTask* task) {
    assert(task != NULL);

    task->state = TASK_INACTIVE;
    task->remaining_time = task->execution_time;
    task->absolute_deadline = task->arrival_time + task->deadline;
    task->next_arrival = task->arrival_time;
    task->completed_jobs = 0;
    task->missed_jobs = 0;
    task->preempted_count = 0;
    task->total_executed = 0.0;
    task->wait_time = 0.0;
    task->response_count = 0;
    task->sum_response = 0.0;
    task->max_response = 0.0;
    task->min_response = DBL_MAX;
}

void task_on_arrival(RealTimeTask* task, double current_time) {
    assert(task != NULL);

    task->remaining_time = task->execution_time;
    task->absolute_deadline = current_time + task->deadline;
    task->next_arrival = current_time + task->period;
    task->state = TASK_READY;
    task->wait_time = 0.0;
}

double task_execute(RealTimeTask* task, double time_slice) {
    assert(task != NULL);
    assert(time_slice > 0);

    double executed = (time_slice < task->remaining_time) ? time_slice : task->remaining_time;
    task->remaining_time -= executed;
    task->total_executed += executed;

    if (task->remaining_time <= EPSILON) {
        task->state = TASK_COMPLETED;
        task->completed_jobs++;
    }

    return executed;
}

void task_preempt(RealTimeTask* task) {
    assert(task != NULL);
    if (task->state == TASK_RUNNING) {
        task->state = TASK_PREEMPTED;
        task->preempted_count++;
    }
}

void task_resume(RealTimeTask* task) {
    assert(task != NULL);
    if (task->state == TASK_READY || task->state == TASK_PREEMPTED) {
        task->state = TASK_RUNNING;
    }
}

bool task_check_deadline_miss(RealTimeTask* task, double current_time) {
    assert(task != NULL);

    if (current_time >= task->absolute_deadline - EPSILON && task->remaining_time > EPSILON) {
        if (task->state != TASK_MISSED) {
            task->state = TASK_MISSED;
            task->missed_jobs++;
            return true;
        }
    }
    return false;
}

void task_record_response(RealTimeTask* task, double response_time) {
    assert(task != NULL);
    if (task->response_count < MAX_EVENTS) {
        task->response_times[task->response_count++] = response_time;
        task->sum_response += response_time;
        if (response_time > task->max_response) task->max_response = response_time;
        if (response_time < task->min_response) task->min_response = response_time;
    }
}

void task_print(const RealTimeTask* task) {
    assert(task != NULL);
    printf("  [%s] id=%d, T=%.2f, C=%.2f, D=%.2f, U=%.4f, state=%d\n",
           task->name, task->task_id, task->period, task->execution_time,
           task->deadline, task_utilization(task), task->state);
}

void taskset_init(TaskSet* ts, const char* name) {
    assert(ts != NULL);
    ts->count = 0;
    if (name) {
        strncpy(ts->name, name, MAX_NAME_LEN - 1);
        ts->name[MAX_NAME_LEN - 1] = '\0';
    } else {
        strcpy(ts->name, "unnamed");
    }
}

bool taskset_add(TaskSet* ts, const RealTimeTask* task) {
    assert(ts != NULL && task != NULL);
    if (ts->count >= MAX_TASKS) {
        fprintf(stderr, "Error: TaskSet full (max %d)\n", MAX_TASKS);
        return false;
    }
    ts->tasks[ts->count] = *task;
    ts->count++;
    return true;
}

static int cmp_period(const void* a, const void* b) {
    const RealTimeTask* ta = (const RealTimeTask*)a;
    const RealTimeTask* tb = (const RealTimeTask*)b;
    if (ta->period < tb->period) return -1;
    if (ta->period > tb->period) return 1;
    return 0;
}

static int cmp_deadline(const void* a, const void* b) {
    const RealTimeTask* ta = (const RealTimeTask*)a;
    const RealTimeTask* tb = (const RealTimeTask*)b;
    if (ta->deadline < tb->deadline) return -1;
    if (ta->deadline > tb->deadline) return 1;
    return 0;
}

void taskset_sort_by_period(TaskSet* ts) {
    assert(ts != NULL);
    qsort(ts->tasks, ts->count, sizeof(RealTimeTask), cmp_period);
}

void taskset_sort_by_deadline(TaskSet* ts) {
    assert(ts != NULL);
    qsort(ts->tasks, ts->count, sizeof(RealTimeTask), cmp_deadline);
}

void taskset_clone(const TaskSet* src, TaskSet* dst) {
    assert(src != NULL && dst != NULL);
    *dst = *src;
    for (int i = 0; i < dst->count; i++) {
        task_reset(&dst->tasks[i]);
    }
}

void taskset_reset_all(TaskSet* ts) {
    assert(ts != NULL);
    for (int i = 0; i < ts->count; i++) {
        task_reset(&ts->tasks[i]);
    }
}

void taskset_print(const TaskSet* ts) {
    assert(ts != NULL);
    printf("TaskSet: %s (count=%d, U=%.4f)\n", ts->name, ts->count, taskset_total_utilization(ts));
    for (int i = 0; i < ts->count; i++) {
        task_print(&ts->tasks[i]);
    }
}

void taskset_generate_random(TaskSet* ts, int n,
                              double min_period, double max_period,
                              double target_u, unsigned int seed) {
    assert(ts != NULL);
    assert(n > 0 && n <= MAX_TASKS);
    assert(min_period > 0 && max_period >= min_period);

    taskset_init(ts, "random");
    srand(seed);

    double periods[MAX_TASKS];
    for (int i = 0; i < n; i++) {
        double r = (double)rand() / RAND_MAX;
        periods[i] = min_period + r * (max_period - min_period);
        periods[i] = round(periods[i] * 10.0) / 10.0;
        if (periods[i] < 1.0) periods[i] = 1.0;
    }

    double utilizations[MAX_TASKS];
    double sum_u = target_u;
    for (int i = 0; i < n - 1; i++) {
        double r = pow((double)rand() / RAND_MAX, 1.0 / (n - i));
        utilizations[i] = sum_u * (1.0 - r);
        sum_u -= utilizations[i];
    }
    utilizations[n - 1] = sum_u;

    for (int i = 0; i < n; i++) {
        double wcet = utilizations[i] * periods[i];
        if (wcet < 0.1) wcet = 0.1;
        wcet = round(wcet * 10.0) / 10.0;
        if (wcet > periods[i]) wcet = periods[i];

        RealTimeTask t;
        char name[MAX_NAME_LEN];
        snprintf(name, MAX_NAME_LEN, "T%d", i + 1);
        task_init(&t, i, periods[i], wcet, periods[i], 0.0, name);
        taskset_add(ts, &t);
    }
}

void taskset_generate_rms_friendly(TaskSet* ts, int n, double utilization) {
    assert(ts != NULL && n > 0);
    taskset_init(ts, "rms-friendly");

    double base_period = 10.0;
    double sum_u = 0.0;

    for (int i = 0; i < n && sum_u < utilization; i++) {
        double period = base_period * pow(2.0, i);
        double remaining = utilization - sum_u;
        double u = (remaining < 0.3) ? remaining : 0.3;
        if (i == n - 1) u = remaining;
        if (u < 0.05) u = 0.05;

        double wcet = u * period;
        if (wcet < 0.5) wcet = 0.5;

        RealTimeTask t;
        char name[MAX_NAME_LEN];
        snprintf(name, MAX_NAME_LEN, "RT%d", i + 1);
        task_init(&t, i, period, wcet, period, 0.0, name);
        taskset_add(ts, &t);
        sum_u += task_utilization(&t);
    }
}

void taskset_generate_edf_critical(TaskSet* ts, int n) {
    assert(ts != NULL && n > 0);
    taskset_init(ts, "edf-critical");

    double period = 100.0;
    double total_u = 0.95;

    for (int i = 0; i < n - 1; i++) {
        double u = total_u / n;
        double wcet = u * period;
        RealTimeTask t;
        char name[MAX_NAME_LEN];
        snprintf(name, MAX_NAME_LEN, "EC%d", i + 1);
        task_init(&t, i, period, wcet, period, 0.0, name);
        taskset_add(ts, &t);
    }

    double current_u = taskset_total_utilization(ts);
    double last_u = 0.98 - current_u;
    if (last_u < 0.01) last_u = 0.01;
    RealTimeTask t;
    char name[MAX_NAME_LEN];
    snprintf(name, MAX_NAME_LEN, "EC%d", n);
    task_init(&t, n - 1, period, last_u * period, period, 0.0, name);
    taskset_add(ts, &t);
}
