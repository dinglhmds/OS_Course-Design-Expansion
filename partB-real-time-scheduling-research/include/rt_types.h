/**
 * @file rt_types.h
 * @brief Core type definitions for real-time scheduling system
 */

#ifndef RT_TYPES_H
#define RT_TYPES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

#define MAX_TASKS           64
#define MAX_EVENTS          2000
#define MAX_NAME_LEN        64
#define EPSILON             1e-9
#define DEFAULT_SIM_TIME    1000.0
#define DEFAULT_TIME_STEP   0.1
#define MAX_ITERATIONS      10000

typedef enum {
    TASK_INACTIVE,
    TASK_READY,
    TASK_RUNNING,
    TASK_COMPLETED,
    TASK_MISSED,
    TASK_PREEMPTED
} TaskState;

typedef enum {
    SCHED_RMS,
    SCHED_EDF,
    SCHED_LLF,
    SCHED_DMS,
    SCHED_FCFS,
    SCHED_SJF,
    SCHED_COUNT
} SchedulerType;

typedef enum {
    RESULT_OK,
    RESULT_ERROR,
    RESULT_NO_MEMORY,
    RESULT_INVALID_ARG,
    RESULT_OVERFLOW
} ResultCode;

typedef struct {
    int     task_id;
    double  period;
    double  execution_time;
    double  deadline;
    double  arrival_time;
    char    name[MAX_NAME_LEN];

    TaskState   state;
    double      remaining_time;
    double      absolute_deadline;
    double      next_arrival;
    int         completed_jobs;
    int         missed_jobs;
    int         preempted_count;
    double      total_executed;
    double      wait_time;
    double      priority;

    double      response_times[MAX_EVENTS];
    int         response_count;
    double      sum_response;
    double      max_response;
    double      min_response;
} RealTimeTask;

typedef struct {
    int     task_id;
    char    task_name[MAX_NAME_LEN];
    double  start_time;
    double  end_time;
    TaskState   state;
} ScheduleEvent;

typedef struct {
    RealTimeTask*   current_task;
    bool            idle;
    double          current_time;
} ProcessorState;

typedef struct {
    RealTimeTask    tasks[MAX_TASKS];
    int             count;
    char            name[MAX_NAME_LEN];
} TaskSet;

typedef struct {
    char        scheduler_name[MAX_NAME_LEN];
    char        task_set_name[MAX_NAME_LEN];
    double      simulation_time;
    double      time_step;

    ScheduleEvent   events[MAX_EVENTS];
    int             event_count;

    int     total_jobs_completed;
    int     total_jobs_missed;
    int     total_preemptions;
    int     total_context_switches;

    double  avg_cpu_utilization;
    double  total_busy_time;
    double  total_idle_time;

    double  deadline_miss_ratio;
    double  max_response_time;
    double  avg_response_time;

    double  task_avg_response[MAX_TASKS];
    double  task_max_response[MAX_TASKS];
    double  task_missed[MAX_TASKS];
    int     task_job_count[MAX_TASKS];
} SimulationResult;

typedef struct {
    bool    schedulable;
    char    method_used[MAX_NAME_LEN];
    double  utilization;
    double  bound;
    char    details[1024];
} SchedulabilityResult;

static inline double task_utilization(const RealTimeTask* task) {
    if (task->period <= EPSILON) return 0.0;
    return task->execution_time / task->period;
}

static inline double task_slack(const RealTimeTask* task, double current_time) {
    return task->absolute_deadline - current_time - task->remaining_time;
}

static inline bool task_is_arrival(const RealTimeTask* task, double current_time) {
    if (current_time < task->next_arrival - EPSILON) return false;
    return (current_time >= task->next_arrival - EPSILON);
}

static inline double taskset_total_utilization(const TaskSet* ts) {
    double u = 0.0;
    for (int i = 0; i < ts->count; i++) {
        u += task_utilization(&ts->tasks[i]);
    }
    return u;
}

static inline double taskset_rms_bound(const TaskSet* ts) {
    int n = ts->count;
    if (n <= 0) return 0.0;
    return (double)n * (pow(2.0, 1.0 / (double)n) - 1.0);
}

#endif
