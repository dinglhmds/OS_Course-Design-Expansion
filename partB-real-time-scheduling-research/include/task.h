/**
 * @file task.h
 * @brief Real-time task management interface
 */

#ifndef TASK_H
#define TASK_H

#include "rt_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void task_init(RealTimeTask* task, int id, double period, double wcet,
               double deadline, double arrival, const char* name);
void task_reset(RealTimeTask* task);
void task_on_arrival(RealTimeTask* task, double current_time);
double task_execute(RealTimeTask* task, double time_slice);
void task_preempt(RealTimeTask* task);
void task_resume(RealTimeTask* task);
bool task_check_deadline_miss(RealTimeTask* task, double current_time);
void task_record_response(RealTimeTask* task, double response_time);
void task_print(const RealTimeTask* task);

void taskset_init(TaskSet* ts, const char* name);
bool taskset_add(TaskSet* ts, const RealTimeTask* task);
void taskset_sort_by_period(TaskSet* ts);
void taskset_sort_by_deadline(TaskSet* ts);
void taskset_clone(const TaskSet* src, TaskSet* dst);
void taskset_reset_all(TaskSet* ts);
void taskset_print(const TaskSet* ts);

void taskset_generate_random(TaskSet* ts, int n,
                              double min_period, double max_period,
                              double target_u, unsigned int seed);
void taskset_generate_rms_friendly(TaskSet* ts, int n, double utilization);
void taskset_generate_edf_critical(TaskSet* ts, int n);

#ifdef __cplusplus
}
#endif

#endif
