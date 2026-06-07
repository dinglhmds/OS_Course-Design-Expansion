/**
 * @file simulator.c
 * @brief Discrete event simulation engine core
 */

#include "scheduler.h"
#include "task.h"
#include <string.h>

void scheduler_init(Scheduler* sched, SchedulerOps* ops) {
    assert(sched != NULL && ops != NULL);
    memset(sched, 0, sizeof(Scheduler));
    sched->ops = ops;
    sched->current_task_idx = -1;
    sched->last_task_id = -1;
}

void scheduler_setup(Scheduler* sched, TaskSet* ts, double sim_time, double time_step) {
    assert(sched != NULL);
    sched->task_set = ts;
    sched->sim_time = sim_time;
    sched->time_step = time_step;
    sched->current_time = 0.0;
    sched->event_count = 0;
    sched->event_start_time = 0.0;
    sched->current_task_idx = -1;
    sched->last_task_id = -1;

    taskset_reset_all(ts);

    if (sched->ops->init_priorities) {
        sched->ops->init_priorities(ts);
    }
}

void scheduler_reset(Scheduler* sched) {
    assert(sched != NULL);
    sched->event_count = 0;
    sched->current_time = 0.0;
    sched->current_task_idx = -1;
    sched->last_task_id = -1;
}

static void record_event(Scheduler* sched, int task_idx, TaskState state, double end_time) {
    if (sched->event_count >= MAX_EVENTS) return;

    ScheduleEvent* ev = &sched->events[sched->event_count++];
    if (task_idx >= 0 && task_idx < sched->task_set->count) {
        ev->task_id = sched->task_set->tasks[task_idx].task_id;
        strncpy(ev->task_name, sched->task_set->tasks[task_idx].name, MAX_NAME_LEN - 1);
        ev->task_name[MAX_NAME_LEN - 1] = '\0';
    } else {
        ev->task_id = -1;
        strcpy(ev->task_name, "IDLE");
    }
    ev->start_time = sched->event_start_time;
    ev->end_time = end_time;
    ev->state = state;
    sched->event_start_time = end_time;
}

static void check_arrivals(Scheduler* sched) {
    TaskSet* ts = sched->task_set;
    for (int i = 0; i < ts->count; i++) {
        RealTimeTask* t = &ts->tasks[i];
        if (task_is_arrival(t, sched->current_time)) {
            task_on_arrival(t, sched->current_time);
            if (sched->ops->on_arrival) {
                sched->ops->on_arrival(t, sched->current_time);
            }
        }
    }
}

static void check_deadlines(Scheduler* sched) {
    TaskSet* ts = sched->task_set;
    for (int i = 0; i < ts->count; i++) {
        RealTimeTask* t = &ts->tasks[i];
        if (t->state == TASK_READY || t->state == TASK_RUNNING || t->state == TASK_PREEMPTED) {
            if (task_check_deadline_miss(t, sched->current_time)) {
                if (sched->ops->on_miss) {
                    sched->ops->on_miss(t);
                }
                if (sched->current_task_idx == i) {
                    sched->current_task_idx = -1;
                }
            }
        }
    }
}

static void switch_context(Scheduler* sched, int new_idx) {
    if (sched->current_task_idx >= 0) {
        RealTimeTask* cur = &sched->task_set->tasks[sched->current_task_idx];
        if (cur->state == TASK_RUNNING) {
            record_event(sched, sched->current_task_idx, TASK_RUNNING, sched->current_time);
            task_preempt(cur);
        }
    } else {
        if (sched->event_start_time < sched->current_time - EPSILON) {
            record_event(sched, -1, TASK_INACTIVE, sched->current_time);
        }
    }

    if (new_idx >= 0) {
        sched->current_task_idx = new_idx;
        task_resume(&sched->task_set->tasks[new_idx]);
    } else {
        sched->current_task_idx = -1;
    }
}

SimulationResult scheduler_run(Scheduler* sched, TaskSet* ts,
                                double sim_time, double time_step) {
    assert(sched != NULL && ts != NULL);

    scheduler_setup(sched, ts, sim_time, time_step);

    while (sched->current_time < sim_time - EPSILON) {
        check_deadlines(sched);
        check_arrivals(sched);

        int next_idx = -1;
        if (sched->ops->select_next) {
            next_idx = sched->ops->select_next(ts, sched->current_time);
        }

        if (sched->ops->preemptive || sched->current_task_idx < 0) {
            if (next_idx != sched->current_task_idx) {
                switch_context(sched, next_idx);
            }
        }

        if (sched->current_task_idx >= 0) {
            RealTimeTask* cur = &ts->tasks[sched->current_task_idx];
            if (cur->state == TASK_RUNNING) {
                double exec_start = sched->current_time;
                double executed = task_execute(cur, time_step);

                if (cur->state == TASK_COMPLETED) {
                    double arrival_of_job = cur->absolute_deadline - cur->deadline;
                    double response = exec_start + executed - arrival_of_job;
                    if (response < 0) response = 0;
                    task_record_response(cur, response);

                    if (sched->ops->on_complete) {
                        sched->ops->on_complete(cur);
                    }

                    sched->current_task_idx = -1;
                }
            }
        }

        sched->current_time += time_step;
    }

    if (sched->current_task_idx >= 0) {
        RealTimeTask* cur = &ts->tasks[sched->current_task_idx];
        if (cur->state == TASK_RUNNING) {
            record_event(sched, sched->current_task_idx, TASK_RUNNING, sim_time);
        }
    } else {
        if (sched->event_start_time < sim_time - EPSILON) {
            record_event(sched, -1, TASK_INACTIVE, sim_time);
        }
    }

    SimulationResult result;
    memset(&result, 0, sizeof(SimulationResult));

    strncpy(result.scheduler_name, sched->ops->name, MAX_NAME_LEN - 1);
    strncpy(result.task_set_name, ts->name, MAX_NAME_LEN - 1);
    result.simulation_time = sim_time;
    result.time_step = time_step;
    result.event_count = sched->event_count;
    memcpy(result.events, sched->events, sizeof(ScheduleEvent) * sched->event_count);

    for (int i = 0; i < ts->count; i++) {
        result.total_jobs_completed += ts->tasks[i].completed_jobs;
        result.total_jobs_missed += ts->tasks[i].missed_jobs;
        result.total_preemptions += ts->tasks[i].preempted_count;

        result.task_avg_response[i] = (ts->tasks[i].response_count > 0)
            ? (ts->tasks[i].sum_response / ts->tasks[i].response_count) : 0.0;
        result.task_max_response[i] = ts->tasks[i].max_response;
        result.task_missed[i] = ts->tasks[i].missed_jobs;
        result.task_job_count[i] = ts->tasks[i].completed_jobs + ts->tasks[i].missed_jobs;
    }

    result.total_context_switches = result.event_count;

    result.total_busy_time = 0.0;
    for (int i = 0; i < result.event_count; i++) {
        if (result.events[i].state == TASK_RUNNING) {
            result.total_busy_time += (result.events[i].end_time - result.events[i].start_time);
        }
    }
    if (sim_time > EPSILON) {
        result.avg_cpu_utilization = result.total_busy_time / sim_time;
    }
    result.total_idle_time = sim_time - result.total_busy_time;

    double total_rt = 0.0;
    int total_rt_count = 0;
    for (int i = 0; i < ts->count; i++) {
        for (int j = 0; j < ts->tasks[i].response_count; j++) {
            double rt = ts->tasks[i].response_times[j];
            total_rt += rt;
            total_rt_count++;
            if (rt > result.max_response_time) result.max_response_time = rt;
        }
    }
    if (total_rt_count > 0) {
        result.avg_response_time = total_rt / total_rt_count;
    }

    int total_jobs = result.total_jobs_completed + result.total_jobs_missed;
    if (total_jobs > 0) {
        result.deadline_miss_ratio = (double)result.total_jobs_missed / (double)total_jobs;
    }

    return result;
}
