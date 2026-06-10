/**
 * @file main.c
 * @brief Main entry for real-time scheduling experiments
 *
 * Runs 10 experiments covering demonstration, comparison,
 * schedulability, overload, scalability, boundary cases,
 * context switch analysis, period sensitivity, unit tests,
 * and performance benchmarks.
 */

#include "scheduler.h"
#include "task.h"
#include "utils.h"
#include <time.h>

#define RESULTS_DIR "results/"

static SchedulerOps* g_schedulers[SCHED_COUNT] = {
    &g_rms_ops, &g_edf_ops, &g_llf_ops, &g_dms_ops, &g_fcfs_ops, &g_sjf_ops
};

static void ensure_dir(const char* path) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", path);
    (void)system(cmd);
}

static void print_separator(void) {
    printf("\n================================================================================\n");
}

static void print_header(const char* title) {
    print_separator();
    printf("  %s\n", title);
    print_separator();
}

static void print_result_console(const SimulationResult* r) {
    printf("\n[%s] on [%s] (T=%.1f, dt=%.3f)\n",
           r->scheduler_name, r->task_set_name, r->simulation_time, r->time_step);
    printf("  Completed: %d | Missed: %d | MissRatio: %.4f | Preempt: %d | CS: %d\n",
           r->total_jobs_completed, r->total_jobs_missed, r->deadline_miss_ratio,
           r->total_preemptions, r->total_context_switches);
    printf("  CPU Util:  %.4f | Busy: %.2f | Idle: %.2f\n",
           r->avg_cpu_utilization, r->total_busy_time, r->total_idle_time);
    printf("  AvgResp:   %.4f | MaxResp: %.4f\n",
           r->avg_response_time, r->max_response_time);
}

static void export_gantt_csv(const SimulationResult* r, const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) return;

    fprintf(fp, "task_id,task_name,start,end,duration,state\n");
    for (int i = 0; i < r->event_count; i++) {
        const ScheduleEvent* e = &r->events[i];
        fprintf(fp, "%d,%s,%.4f,%.4f,%.4f,%d\n",
                e->task_id, e->task_name, e->start_time, e->end_time,
                e->end_time - e->start_time, e->state);
    }
    fclose(fp);
}

static void export_metrics_csv(const SimulationResult* results, int count, const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) return;

    fprintf(fp, "scheduler,completed,missed,miss_ratio,preemptions,context_switches,"
                "cpu_util,busy_time,idle_time,avg_response,max_response\n");

    for (int i = 0; i < count; i++) {
        const SimulationResult* r = &results[i];
        fprintf(fp, "%s,%d,%d,%.6f,%d,%d,%.6f,%.2f,%.2f,%.4f,%.4f\n",
                r->scheduler_name, r->total_jobs_completed, r->total_jobs_missed,
                r->deadline_miss_ratio, r->total_preemptions, r->total_context_switches,
                r->avg_cpu_utilization, r->total_busy_time, r->total_idle_time,
                r->avg_response_time, r->max_response_time);
    }
    fclose(fp);
}

static void experiment_basic(void) {
    print_header("Exp 1: Basic Scheduling Demonstration");

    TaskSet ts;
    taskset_init(&ts, "basic-demo");

    RealTimeTask t1, t2, t3;
    task_init(&t1, 0, 10.0, 3.0, 10.0, 0.0, "T1-high");
    task_init(&t2, 1, 20.0, 5.0, 20.0, 0.0, "T2-mid");
    task_init(&t3, 2, 30.0, 6.0, 30.0, 0.0, "T3-low");
    taskset_add(&ts, &t1);
    taskset_add(&ts, &t2);
    taskset_add(&ts, &t3);

    taskset_print(&ts);
    printf("Total Utilization: %.4f\n", taskset_total_utilization(&ts));
    printf("RMS Bound (n=3): %.4f\n", taskset_rms_bound(&ts));

    TaskSet ts_copy;
    taskset_clone(&ts, &ts_copy);

    Scheduler sched;
    scheduler_init(&sched, &g_rms_ops);
    SimulationResult r = scheduler_run(&sched, &ts_copy, 60.0, 0.1);
    print_result_console(&r);
    export_gantt_csv(&r, RESULTS_DIR "exp1_rms_gantt.csv");

    taskset_clone(&ts, &ts_copy);
    scheduler_init(&sched, &g_edf_ops);
    r = scheduler_run(&sched, &ts_copy, 60.0, 0.1);
    print_result_console(&r);
    export_gantt_csv(&r, RESULTS_DIR "exp1_edf_gantt.csv");
}

static void experiment_comparison(void) {
    print_header("Exp 2: Algorithm Comparison");

    TaskSet ts;
    taskset_generate_random(&ts, 5, 5.0, 50.0, 0.75, 42);
    taskset_print(&ts);

    SimulationResult results[SCHED_COUNT];

    for (int s = 0; s < SCHED_COUNT; s++) {
        TaskSet ts_copy;
        taskset_clone(&ts, &ts_copy);

        Scheduler sched;
        scheduler_init(&sched, g_schedulers[s]);
        results[s] = scheduler_run(&sched, &ts_copy, 200.0, 0.1);
        print_result_console(&results[s]);
    }

    export_metrics_csv(results, SCHED_COUNT, RESULTS_DIR "exp2_comparison.csv");

    printf("\n  [Schedulability Analysis]\n");
    SchedulabilityResult sr = schedulability_analyze(&ts, SCHED_RMS);
    printf("  RMS LL Test:      %s (U=%.4f, bound=%.4f)\n",
           sr.schedulable ? "PASS" : "FAIL", sr.utilization, sr.bound);
    sr = schedulability_analyze(&ts, SCHED_EDF);
    printf("  EDF Util Test:    %s (U=%.4f)\n",
           sr.schedulable ? "PASS" : "FAIL", sr.utilization);
    sr = schedulability_analyze(&ts, SCHED_DMS);
    printf("  DMS RTA:          %s\n", sr.schedulable ? "PASS" : "FAIL");
}

static void experiment_schedulability(void) {
    print_header("Exp 3: Schedulability vs Utilization");

    FILE* fp = fopen(RESULTS_DIR "exp3_schedulability.csv", "w");
    if (!fp) return;
    fprintf(fp, "utilization,rms_ll,rms_hyp,rms_rta,edf_util,edf_density,dms_rta\n");

    int trials = 100;

    for (double u = 0.1; u <= 1.0 + EPSILON; u += 0.05) {
        int rms_ll = 0, rms_hyp = 0, rms_rta = 0;
        int edf_u = 0, edf_d = 0, dms_rta = 0;

        for (int t = 0; t < trials; t++) {
            TaskSet ts;
            taskset_generate_random(&ts, 5, 5.0, 50.0, u, (unsigned)(time(NULL) + t));

            if (rms_ll_test(&ts)) rms_ll++;
            if (rms_hyperbolic_test(&ts)) rms_hyp++;
            if (rms_response_time_analysis(&ts, NULL)) rms_rta++;
            if (edf_utilization_test(&ts)) edf_u++;
            if (edf_density_test(&ts)) edf_d++;
            if (dms_response_time_analysis(&ts, NULL)) dms_rta++;
        }

        printf("  U=%.2f: RMS(LL=%.2f RTA=%.2f) EDF(%.2f) DMS(%.2f)\n",
               u,
               (double)rms_ll / trials, (double)rms_rta / trials,
               (double)edf_u / trials, (double)dms_rta / trials);

        fprintf(fp, "%.2f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n",
                u,
                (double)rms_ll / trials, (double)rms_hyp / trials, (double)rms_rta / trials,
                (double)edf_u / trials, (double)edf_d / trials, (double)dms_rta / trials);
    }

    fclose(fp);
}

static void experiment_overload(void) {
    print_header("Exp 4: Overload Scenario");

    FILE* fp = fopen(RESULTS_DIR "exp4_overload.csv", "w");
    if (!fp) return;
    fprintf(fp, "utilization,scheduler,miss_ratio,preemptions,context_switches\n");

    for (double u = 0.6; u <= 1.4 + EPSILON; u += 0.1) {
        TaskSet ts;
        taskset_generate_random(&ts, 6, 5.0, 40.0, u, 123);

        for (int s = 0; s < SCHED_COUNT; s++) {
            TaskSet ts_copy;
            taskset_clone(&ts, &ts_copy);

            Scheduler sched;
            scheduler_init(&sched, g_schedulers[s]);
            SimulationResult r = scheduler_run(&sched, &ts_copy, 300.0, 0.1);

            printf("  U=%.2f %s: miss=%.4f preempt=%d\n",
                   u, r.scheduler_name, r.deadline_miss_ratio, r.total_preemptions);

            fprintf(fp, "%.2f,%s,%.6f,%d,%d\n",
                    u, r.scheduler_name, r.deadline_miss_ratio,
                    r.total_preemptions, r.total_context_switches);
        }
    }

    fclose(fp);
}

static void experiment_scalability(void) {
    print_header("Exp 5: Scalability Test");

    FILE* fp = fopen(RESULTS_DIR "exp5_scalability.csv", "w");
    if (!fp) return;
    fprintf(fp, "task_count,scheduler,avg_response,max_response,miss_ratio,cpu_util\n");

    for (int n = 2; n <= 20; n += 2) {
        TaskSet ts;
        taskset_generate_random(&ts, n, 5.0, 50.0, 0.7, n * 7);

        for (int s = 0; s < SCHED_COUNT; s++) {
            TaskSet ts_copy;
            taskset_clone(&ts, &ts_copy);

            Scheduler sched;
            scheduler_init(&sched, g_schedulers[s]);
            SimulationResult r = scheduler_run(&sched, &ts_copy, 200.0, 0.1);

            fprintf(fp, "%d,%s,%.4f,%.4f,%.6f,%.4f\n",
                    n, r.scheduler_name, r.avg_response_time, r.max_response_time,
                    r.deadline_miss_ratio, r.avg_cpu_utilization);
        }

        printf("  n=%2d: generated tasks, U=%.4f\n", n, taskset_total_utilization(&ts));
    }

    fclose(fp);
}

static void experiment_boundary(void) {
    print_header("Exp 6: Boundary Case (RMS vs EDF)");

    TaskSet ts;
    taskset_init(&ts, "boundary-rms-edf");

    RealTimeTask t1, t2, t3;
    task_init(&t1, 0, 4.0, 2.0, 4.0, 0.0, "T1-fast");
    task_init(&t2, 1, 10.0, 3.0, 10.0, 0.0, "T2-mid");
    task_init(&t3, 2, 20.0, 4.0, 20.0, 0.0, "T3-slow");
    taskset_add(&ts, &t1);
    taskset_add(&ts, &t2);
    taskset_add(&ts, &t3);

    taskset_print(&ts);
    printf("Total U = %.4f, RMS bound = %.4f\n",
           taskset_total_utilization(&ts), taskset_rms_bound(&ts));

    SimulationResult results[SCHED_COUNT];
    for (int s = 0; s < SCHED_COUNT; s++) {
        TaskSet ts_copy;
        taskset_clone(&ts, &ts_copy);

        Scheduler sched;
        scheduler_init(&sched, g_schedulers[s]);
        results[s] = scheduler_run(&sched, &ts_copy, 100.0, 0.05);
        print_result_console(&results[s]);
    }

    export_metrics_csv(results, SCHED_COUNT, RESULTS_DIR "exp6_boundary.csv");
}

static void experiment_context_switch(void) {
    print_header("Exp 7: Context Switch Analysis");

    FILE* fp = fopen(RESULTS_DIR "exp7_context_switch.csv", "w");
    if (!fp) return;
    fprintf(fp, "scheduler,context_switches,preemptions,events\n");

    TaskSet ts;
    taskset_init(&ts, "high-freq");

    RealTimeTask tasks[4];
    task_init(&tasks[0], 0, 3.0, 0.8, 3.0, 0.0, "HF1");
    task_init(&tasks[1], 1, 5.0, 1.2, 5.0, 0.0, "HF2");
    task_init(&tasks[2], 2, 7.0, 1.5, 7.0, 0.0, "HF3");
    task_init(&tasks[3], 3, 11.0, 2.0, 11.0, 0.0, "HF4");
    for (int i = 0; i < 4; i++) taskset_add(&ts, &tasks[i]);

    for (int s = 0; s < SCHED_COUNT; s++) {
        TaskSet ts_copy;
        taskset_clone(&ts, &ts_copy);

        Scheduler sched;
        scheduler_init(&sched, g_schedulers[s]);
        SimulationResult r = scheduler_run(&sched, &ts_copy, 200.0, 0.05);

        printf("  %s: CS=%d, Preempt=%d, Events=%d\n",
               r.scheduler_name, r.total_context_switches,
               r.total_preemptions, r.event_count);

        fprintf(fp, "%s,%d,%d,%d\n",
                r.scheduler_name, r.total_context_switches,
                r.total_preemptions, r.event_count);
    }

    fclose(fp);
}

static void experiment_period_sensitivity(void) {
    print_header("Exp 8: Period Sensitivity");

    FILE* fp = fopen(RESULTS_DIR "exp8_period_sensitivity.csv", "w");
    if (!fp) return;
    fprintf(fp, "period_ratio,scheduler,miss_ratio,avg_response\n");

    double base_period = 10.0;
    for (double ratio = 0.5; ratio <= 3.0 + EPSILON; ratio += 0.25) {
        TaskSet ts;
        taskset_init(&ts, "period-sens");

        RealTimeTask t1, t2;
        task_init(&t1, 0, base_period * ratio, base_period * ratio * 0.3,
                  base_period * ratio, 0.0, "T1");
        task_init(&t2, 1, base_period * ratio * 2.0, base_period * ratio * 0.35,
                  base_period * ratio * 2.0, 0.0, "T2");
        taskset_add(&ts, &t1);
        taskset_add(&ts, &t2);

        for (int s = 0; s < SCHED_COUNT; s++) {
            TaskSet ts_copy;
            taskset_clone(&ts, &ts_copy);

            Scheduler sched;
            scheduler_init(&sched, g_schedulers[s]);
            SimulationResult r = scheduler_run(&sched, &ts_copy, 200.0, 0.1);

            fprintf(fp, "%.2f,%s,%.6f,%.4f\n",
                    ratio, r.scheduler_name, r.deadline_miss_ratio, r.avg_response_time);
        }
    }

    fclose(fp);
}

static void experiment_unit_test(void) {
    print_header("Exp 9: Unit Tests & Correctness Verification");

    FILE* fp = fopen(RESULTS_DIR "exp9_unit_test.csv", "w");
    if (!fp) return;
    fprintf(fp, "category,test_name,result\n");

    int passed = 0, failed = 0;
    Timer timer;
    timer_start(&timer);

    printf("\n  [Task Management Tests]\n");

    {
        RealTimeTask t;
        task_init(&t, 1, 10.0, 3.0, 10.0, 0.0, "TestTask");
        bool ok = (t.task_id == 1 && t.period == 10.0 && t.execution_time == 3.0
                   && t.state == TASK_INACTIVE && t.remaining_time == 3.0);
        printf("    task_init ............... %s\n", ok ? "PASS" : "FAIL");
        fprintf(fp, "Task Management,task_init,%s\n", ok ? "PASS" : "FAIL");
        if (ok) passed++; else failed++;
    }

    {
        RealTimeTask t;
        task_init(&t, 1, 10.0, 2.5, 10.0, 0.0, "T");
        bool ok = fabs(task_utilization(&t) - 0.25) < EPSILON;
        printf("    task_utilization ........ %s\n", ok ? "PASS" : "FAIL");
        fprintf(fp, "Task Management,task_utilization,%s\n", ok ? "PASS" : "FAIL");
        if (ok) passed++; else failed++;
    }

    {
        RealTimeTask t;
        task_init(&t, 1, 10.0, 5.0, 10.0, 0.0, "T");
        task_on_arrival(&t, 0.0);
        task_resume(&t);
        double exec = task_execute(&t, 2.0);
        bool ok = (exec == 2.0 && t.remaining_time == 3.0 && t.state == TASK_RUNNING);
        exec = task_execute(&t, 5.0);
        ok = ok && (exec == 3.0 && t.state == TASK_COMPLETED);
        printf("    task_execute ............ %s\n", ok ? "PASS" : "FAIL");
        fprintf(fp, "Task Management,task_execute,%s\n", ok ? "PASS" : "FAIL");
        if (ok) passed++; else failed++;
    }

    {
        RealTimeTask t;
        task_init(&t, 1, 10.0, 5.0, 10.0, 0.0, "T");
        task_on_arrival(&t, 0.0);
        bool ok = !task_check_deadline_miss(&t, 5.0);
        ok = ok && task_check_deadline_miss(&t, 15.0);
        ok = ok && (t.state == TASK_MISSED);
        printf("    task_deadline_miss ...... %s\n", ok ? "PASS" : "FAIL");
        fprintf(fp, "Task Management,task_deadline_miss,%s\n", ok ? "PASS" : "FAIL");
        if (ok) passed++; else failed++;
    }

    printf("\n  [TaskSet Tests]\n");

    {
        TaskSet ts;
        taskset_init(&ts, "test");
        RealTimeTask t1, t2;
        task_init(&t1, 0, 10.0, 2.0, 10.0, 0.0, "T1");
        task_init(&t2, 1, 20.0, 4.0, 20.0, 0.0, "T2");
        taskset_add(&ts, &t1);
        taskset_add(&ts, &t2);
        bool ok = fabs(taskset_total_utilization(&ts) - 0.4) < EPSILON;
        printf("    taskset_utilization ..... %s\n", ok ? "PASS" : "FAIL");
        fprintf(fp, "TaskSet,taskset_utilization,%s\n", ok ? "PASS" : "FAIL");
        if (ok) passed++; else failed++;
    }

    {
        TaskSet ts;
        taskset_init(&ts, "test");
        for (int i = 0; i < 3; i++) {
            RealTimeTask t;
            task_init(&t, i, 10.0 * (i+1), 1.0, 10.0 * (i+1), 0.0, "T");
            taskset_add(&ts, &t);
        }
        bool ok = fabs(taskset_rms_bound(&ts) - 0.7798) < 0.001;
        printf("    taskset_rms_bound ....... %s\n", ok ? "PASS" : "FAIL");
        fprintf(fp, "TaskSet,taskset_rms_bound,%s\n", ok ? "PASS" : "FAIL");
        if (ok) passed++; else failed++;
    }

    printf("\n  [RMS Schedulability Tests]\n");

    {
        TaskSet ts;
        taskset_init(&ts, "pass");
        RealTimeTask t1, t2;
        task_init(&t1, 0, 4.0, 1.0, 4.0, 0.0, "T1");
        task_init(&t2, 1, 6.0, 1.0, 6.0, 0.0, "T2");
        taskset_add(&ts, &t1);
        taskset_add(&ts, &t2);
        bool ok = rms_ll_test(&ts);
        printf("    rms_ll_test_pass ........ %s\n", ok ? "PASS" : "FAIL");
        fprintf(fp, "RMS,rms_ll_test_pass,%s\n", ok ? "PASS" : "FAIL");
        if (ok) passed++; else failed++;
    }

    {
        TaskSet ts;
        taskset_init(&ts, "fail");
        RealTimeTask t1, t2, t3;
        task_init(&t1, 0, 4.0, 2.0, 4.0, 0.0, "T1");
        task_init(&t2, 1, 5.0, 2.0, 5.0, 0.0, "T2");
        task_init(&t3, 2, 10.0, 3.0, 10.0, 0.0, "T3");
        taskset_add(&ts, &t1);
        taskset_add(&ts, &t2);
        taskset_add(&ts, &t3);
        bool ok = !rms_ll_test(&ts);
        printf("    rms_ll_test_fail ........ %s\n", ok ? "PASS" : "FAIL");
        fprintf(fp, "RMS,rms_ll_test_fail,%s\n", ok ? "PASS" : "FAIL");
        if (ok) passed++; else failed++;
    }

    {
        TaskSet ts;
        taskset_init(&ts, "rta");
        RealTimeTask t1, t2;
        task_init(&t1, 0, 4.0, 1.0, 4.0, 0.0, "T1");
        task_init(&t2, 1, 6.0, 2.0, 6.0, 0.0, "T2");
        taskset_add(&ts, &t1);
        taskset_add(&ts, &t2);
        double results[MAX_TASKS];
        bool ok = rms_response_time_analysis(&ts, results);
        ok = ok && (results[0] <= 4.0 + EPSILON);
        ok = ok && (results[1] <= 6.0 + EPSILON);
        printf("    rms_rta_exact ........... %s\n", ok ? "PASS" : "FAIL");
        fprintf(fp, "RMS,rms_rta_exact,%s\n", ok ? "PASS" : "FAIL");
        if (ok) passed++; else failed++;
    }

    printf("\n  [EDF Schedulability Tests]\n");
    {
        TaskSet ts;
        taskset_init(&ts, "edf");
        RealTimeTask t1, t2;
        task_init(&t1, 0, 10.0, 3.0, 10.0, 0.0, "T1");
        task_init(&t2, 1, 20.0, 5.0, 20.0, 0.0, "T2");
        taskset_add(&ts, &t1);
        taskset_add(&ts, &t2);
        bool ok = edf_utilization_test(&ts);
        printf("    edf_util_test ........... %s\n", ok ? "PASS" : "FAIL");
        fprintf(fp, "EDF,edf_util_test,%s\n", ok ? "PASS" : "FAIL");
        if (ok) passed++; else failed++;
    }

    printf("\n  [Simulator Tests]\n");
    {
        TaskSet ts;
        taskset_init(&ts, "sim");
        RealTimeTask t1;
        task_init(&t1, 0, 10.0, 3.0, 10.0, 0.0, "T1");
        taskset_add(&ts, &t1);
        Scheduler sched;
        scheduler_init(&sched, &g_rms_ops);
        SimulationResult r = scheduler_run(&sched, &ts, 20.0, 1.0);
        bool ok = (r.total_jobs_completed == 2 && r.total_jobs_missed == 0);
        printf("    simulator_rms_basic ..... %s\n", ok ? "PASS" : "FAIL");
        fprintf(fp, "Simulator,simulator_rms_basic,%s\n", ok ? "PASS" : "FAIL");
        if (ok) passed++; else failed++;
    }

    {
        TaskSet ts;
        taskset_init(&ts, "sim");
        RealTimeTask t1, t2;
        task_init(&t1, 0, 10.0, 3.0, 10.0, 0.0, "T1");
        task_init(&t2, 1, 20.0, 5.0, 20.0, 0.0, "T2");
        taskset_add(&ts, &t1);
        taskset_add(&ts, &t2);
        Scheduler sched;
        scheduler_init(&sched, &g_edf_ops);
        SimulationResult r = scheduler_run(&sched, &ts, 40.0, 0.5);
        bool ok = (r.total_jobs_completed == 6 && r.total_jobs_missed == 0);
        printf("    simulator_edf_basic ..... %s\n", ok ? "PASS" : "FAIL");
        fprintf(fp, "Simulator,simulator_edf_basic,%s\n", ok ? "PASS" : "FAIL");
        if (ok) passed++; else failed++;
    }

    {
        TaskSet ts;
        taskset_init(&ts, "overload");
        RealTimeTask t1, t2;
        task_init(&t1, 0, 10.0, 7.0, 10.0, 0.0, "T1");
        task_init(&t2, 1, 10.0, 8.0, 10.0, 0.0, "T2");
        taskset_add(&ts, &t1);
        taskset_add(&ts, &t2);
        Scheduler sched;
        scheduler_init(&sched, &g_edf_ops);
        SimulationResult r = scheduler_run(&sched, &ts, 100.0, 0.1);
        bool ok = (r.total_jobs_missed > 0);
        printf("    simulator_edf_overload .. %s\n", ok ? "PASS" : "FAIL");
        fprintf(fp, "Simulator,simulator_edf_overload,%s\n", ok ? "PASS" : "FAIL");
        if (ok) passed++; else failed++;
    }

    printf("\n  [Utility Tests]\n");
    {
        bool ok = (math_gcd(12.0, 8.0) == 4.0);
        ok = ok && (math_gcd(17.0, 13.0) == 1.0);
        ok = ok && (math_lcm(4.0, 6.0) == 12.0);
        ok = ok && (math_lcm(5.0, 7.0) == 35.0);
        printf("    math_gcd_lcm ............ %s\n", ok ? "PASS" : "FAIL");
        fprintf(fp, "Utility,math_gcd_lcm,%s\n", ok ? "PASS" : "FAIL");
        if (ok) passed++; else failed++;
    }

    {
        bool ok = fabs(math_round4(3.1415926) - 3.1416) < EPSILON;
        printf("    math_round4 ............. %s\n", ok ? "PASS" : "FAIL");
        fprintf(fp, "Utility,math_round4,%s\n", ok ? "PASS" : "FAIL");
        if (ok) passed++; else failed++;
    }

    timer_stop(&timer);

    printf("\n  Tests complete: %d passed, %d failed, %.3f ms\n",
           passed, failed, timer_elapsed_ms(&timer));

    fprintf(fp, "Summary,total_passed,%d\n", passed);
    fprintf(fp, "Summary,total_failed,%d\n", failed);
    fprintf(fp, "Summary,elapsed_ms,%.3f\n", timer_elapsed_ms(&timer));
    fclose(fp);

    if (failed > 0) {
        printf("  [WARNING] Some tests failed, check code!\n");
    }
}

static void experiment_performance(void) {
    print_header("Exp 10: Performance Benchmark");

    FILE* fp = fopen(RESULTS_DIR "exp10_performance.csv", "w");
    if (!fp) return;
    fprintf(fp, "scheduler,sim_time_ms,tasks,speed_factor\n");

    TaskSet ts;
    taskset_generate_random(&ts, 10, 5.0, 50.0, 0.8, 999);

    printf("  Task set: %d tasks, U=%.4f\n", ts.count, taskset_total_utilization(&ts));
    printf("  Sim time: 500.0, step: 0.05\n\n");

    double baseline_ms = -1.0;

    for (int s = 0; s < SCHED_COUNT; s++) {
        TaskSet ts_copy;
        taskset_clone(&ts, &ts_copy);

        Timer timer;
        timer_start(&timer);

        Scheduler sched;
        scheduler_init(&sched, g_schedulers[s]);
        SimulationResult r = scheduler_run(&sched, &ts_copy, 500.0, 0.05);

        timer_stop(&timer);
        double ms = timer_elapsed_ms(&timer);

        if (baseline_ms < 0) baseline_ms = ms;
        double speed_factor = baseline_ms / ms;

        printf("  %-6s: %8.3f ms | Speed: %.2fx | Completed: %d | Missed: %d\n",
               r.scheduler_name, ms, speed_factor,
               r.total_jobs_completed, r.total_jobs_missed);

        fprintf(fp, "%s,%.3f,%d,%.4f\n", r.scheduler_name, ms, ts.count, speed_factor);
    }

    fclose(fp);
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    printf("\n");
    printf("+------------------------------------------------------------------------------+\n");
    printf("|                                                                              |\n");
    printf("|          OS Course Project - Real-Time Scheduling Mechanism Research         |\n");
    printf("|                                                                              |\n");
    printf("|          Team Size: 4     Responsibility: Real-Time Scheduling               |\n");
    printf("|                                                                              |\n");
    printf("+------------------------------------------------------------------------------+\n");

    ensure_dir(RESULTS_DIR);

    clock_t total_start = clock();

    experiment_basic();
    experiment_comparison();
    experiment_schedulability();
    experiment_overload();
    experiment_scalability();
    experiment_boundary();
    experiment_context_switch();
    experiment_period_sensitivity();
    experiment_unit_test();
    experiment_performance();

    clock_t total_end = clock();
    double total_time = (double)(total_end - total_start) / CLOCKS_PER_SEC;

    print_separator();
    printf("  All 10 experiments completed! Total time: %.3f seconds\n", total_time);
    printf("  Results saved in: %s\n", RESULTS_DIR);
    printf("  Use visualization/plot.py to generate plots\n");
    print_separator();
    printf("\n");

    return 0;
}
