/**
 * @file scheduler.h
 * @brief Real-time scheduler interface
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "rt_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SchedulerOps {
    const char* name;
    bool        preemptive;

    void (*init_priorities)(TaskSet* ts);
    int (*select_next)(TaskSet* ts, double current_time);
    void (*on_arrival)(RealTimeTask* task, double current_time);
    void (*on_complete)(RealTimeTask* task);
    void (*on_miss)(RealTimeTask* task);
} SchedulerOps;

typedef struct {
    SchedulerOps*   ops;
    TaskSet*        task_set;
    int             current_task_idx;
    double          current_time;
    double          time_step;
    double          sim_time;
    ScheduleEvent   events[MAX_EVENTS];
    int             event_count;
    double          event_start_time;
    int             last_task_id;
} Scheduler;

void scheduler_init(Scheduler* sched, SchedulerOps* ops);
void scheduler_setup(Scheduler* sched, TaskSet* ts, double sim_time, double time_step);
void scheduler_reset(Scheduler* sched);
SimulationResult scheduler_run(Scheduler* sched, TaskSet* ts,
                                double sim_time, double time_step);

extern SchedulerOps g_rms_ops;
extern SchedulerOps g_edf_ops;
extern SchedulerOps g_llf_ops;
extern SchedulerOps g_dms_ops;
extern SchedulerOps g_fcfs_ops;
extern SchedulerOps g_sjf_ops;

bool rms_ll_test(const TaskSet* ts);
bool rms_hyperbolic_test(const TaskSet* ts);
bool rms_response_time_analysis(const TaskSet* ts, double* results);
bool edf_utilization_test(const TaskSet* ts);
bool edf_density_test(const TaskSet* ts);
bool edf_processor_demand_test(const TaskSet* ts);
bool dms_response_time_analysis(const TaskSet* ts, double* results);
SchedulabilityResult schedulability_analyze(const TaskSet* ts, SchedulerType type);

#ifdef __cplusplus
}
#endif

#endif
