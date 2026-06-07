/**
 * @file test_basic.c
 * @brief Basic unit tests
 */

#include "scheduler.h"
#include "task.h"
#include "utils.h"
#include <assert.h>

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST(name) void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  Running " #name " ... "); \
    test_##name(); \
    printf("PASSED\n"); \
    g_tests_passed++; \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("FAILED\n  Assertion failed: %s == %s\n  Expected: %g, Got: %g\n", \
               #a, #b, (double)(b), (double)(a)); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_TRUE(x) do { \
    if (!(x)) { \
        printf("FAILED\n  Assertion failed: %s should be true\n", #x); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_FALSE(x) do { \
    if (x) { \
        printf("FAILED\n  Assertion failed: %s should be false\n", #x); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

TEST(task_init) {
    RealTimeTask t;
    task_init(&t, 1, 10.0, 3.0, 10.0, 0.0, "TestTask");
    ASSERT_EQ(t.task_id, 1);
    ASSERT_EQ(t.period, 10.0);
    ASSERT_EQ(t.execution_time, 3.0);
    ASSERT_EQ(t.deadline, 10.0);
    ASSERT_EQ(t.arrival_time, 0.0);
    ASSERT_EQ(t.state, TASK_INACTIVE);
    ASSERT_EQ(t.remaining_time, 3.0);
}

TEST(task_utilization) {
    RealTimeTask t;
    task_init(&t, 1, 10.0, 2.5, 10.0, 0.0, "T");
    ASSERT_TRUE(fabs(task_utilization(&t) - 0.25) < EPSILON);
}

TEST(task_execute) {
    RealTimeTask t;
    task_init(&t, 1, 10.0, 5.0, 10.0, 0.0, "T");
    task_on_arrival(&t, 0.0);
    task_resume(&t);

    double exec = task_execute(&t, 2.0);
    ASSERT_EQ(exec, 2.0);
    ASSERT_EQ(t.remaining_time, 3.0);
    ASSERT_EQ(t.state, TASK_RUNNING);

    exec = task_execute(&t, 5.0);
    ASSERT_EQ(exec, 3.0);
    ASSERT_EQ(t.state, TASK_COMPLETED);
}

TEST(task_deadline_miss) {
    RealTimeTask t;
    task_init(&t, 1, 10.0, 5.0, 10.0, 0.0, "T");
    task_on_arrival(&t, 0.0);
    ASSERT_FALSE(task_check_deadline_miss(&t, 5.0));
    ASSERT_TRUE(task_check_deadline_miss(&t, 15.0));
    ASSERT_EQ(t.state, TASK_MISSED);
}

TEST(taskset_utilization) {
    TaskSet ts;
    taskset_init(&ts, "test");

    RealTimeTask t1, t2;
    task_init(&t1, 0, 10.0, 2.0, 10.0, 0.0, "T1");
    task_init(&t2, 1, 20.0, 4.0, 20.0, 0.0, "T2");
    taskset_add(&ts, &t1);
    taskset_add(&ts, &t2);

    double u = taskset_total_utilization(&ts);
    ASSERT_TRUE(fabs(u - 0.4) < EPSILON);
}

TEST(taskset_rms_bound) {
    TaskSet ts;
    taskset_init(&ts, "test");
    for (int i = 0; i < 3; i++) {
        RealTimeTask t;
        task_init(&t, i, 10.0 * (i+1), 1.0, 10.0 * (i+1), 0.0, "T");
        taskset_add(&ts, &t);
    }
    double bound = taskset_rms_bound(&ts);
    ASSERT_TRUE(fabs(bound - 0.7798) < 0.001);
}

TEST(rms_ll_test_pass) {
    TaskSet ts;
    taskset_init(&ts, "pass");
    RealTimeTask t1, t2;
    task_init(&t1, 0, 4.0, 1.0, 4.0, 0.0, "T1");
    task_init(&t2, 1, 6.0, 1.0, 6.0, 0.0, "T2");
    taskset_add(&ts, &t1);
    taskset_add(&ts, &t2);
    ASSERT_TRUE(rms_ll_test(&ts));
}

TEST(rms_ll_test_fail) {
    TaskSet ts;
    taskset_init(&ts, "fail");
    RealTimeTask t1, t2, t3;
    task_init(&t1, 0, 4.0, 2.0, 4.0, 0.0, "T1");
    task_init(&t2, 1, 5.0, 2.0, 5.0, 0.0, "T2");
    task_init(&t3, 2, 10.0, 3.0, 10.0, 0.0, "T3");
    taskset_add(&ts, &t1);
    taskset_add(&ts, &t2);
    taskset_add(&ts, &t3);
    ASSERT_FALSE(rms_ll_test(&ts));
}

TEST(rms_rta_exact) {
    TaskSet ts;
    taskset_init(&ts, "rta");
    RealTimeTask t1, t2;
    task_init(&t1, 0, 4.0, 1.0, 4.0, 0.0, "T1");
    task_init(&t2, 1, 6.0, 2.0, 6.0, 0.0, "T2");
    taskset_add(&ts, &t1);
    taskset_add(&ts, &t2);
    double results[MAX_TASKS];
    ASSERT_TRUE(rms_response_time_analysis(&ts, results));
    ASSERT_TRUE(results[0] <= 4.0 + EPSILON);
    ASSERT_TRUE(results[1] <= 6.0 + EPSILON);
}

TEST(edf_util_test) {
    TaskSet ts;
    taskset_init(&ts, "edf");
    RealTimeTask t1, t2;
    task_init(&t1, 0, 10.0, 3.0, 10.0, 0.0, "T1");
    task_init(&t2, 1, 20.0, 5.0, 20.0, 0.0, "T2");
    taskset_add(&ts, &t1);
    taskset_add(&ts, &t2);
    ASSERT_TRUE(edf_utilization_test(&ts));
}

TEST(simulator_rms_basic) {
    TaskSet ts;
    taskset_init(&ts, "sim");
    RealTimeTask t1;
    task_init(&t1, 0, 10.0, 3.0, 10.0, 0.0, "T1");
    taskset_add(&ts, &t1);

    Scheduler sched;
    scheduler_init(&sched, &g_rms_ops);
    SimulationResult r = scheduler_run(&sched, &ts, 20.0, 1.0);
    ASSERT_EQ(r.total_jobs_completed, 2);
    ASSERT_EQ(r.total_jobs_missed, 0);
}

TEST(simulator_edf_basic) {
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
    ASSERT_EQ(r.total_jobs_completed, 6);
    ASSERT_EQ(r.total_jobs_missed, 0);
}

TEST(simulator_edf_overload) {
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
    ASSERT_TRUE(r.total_jobs_missed > 0);
}

TEST(timer_basic) {
    Timer t;
    timer_start(&t);
    for (volatile int i = 0; i < 1000000; i++);
    timer_stop(&t);
    ASSERT_TRUE(t.elapsed_ms >= 0.0);
}

TEST(math_gcd_lcm) {
    ASSERT_EQ(math_gcd(12.0, 8.0), 4.0);
    ASSERT_EQ(math_gcd(17.0, 13.0), 1.0);
    ASSERT_EQ(math_lcm(4.0, 6.0), 12.0);
    ASSERT_EQ(math_lcm(5.0, 7.0), 35.0);
}

TEST(math_round4) {
    ASSERT_TRUE(fabs(math_round4(3.1415926) - 3.1416) < EPSILON);
}

int main(void) {
    printf("\n");
    printf("+================================================================+\n");
    printf("|          Real-Time Scheduling System - Unit Test Suite         |\n");
    printf("+================================================================+\n\n");

    printf("[Task Management Tests]\n");
    RUN_TEST(task_init);
    RUN_TEST(task_utilization);
    RUN_TEST(task_execute);
    RUN_TEST(task_deadline_miss);

    printf("\n[TaskSet Tests]\n");
    RUN_TEST(taskset_utilization);
    RUN_TEST(taskset_rms_bound);

    printf("\n[RMS Schedulability Tests]\n");
    RUN_TEST(rms_ll_test_pass);
    RUN_TEST(rms_ll_test_fail);
    RUN_TEST(rms_rta_exact);

    printf("\n[EDF Schedulability Tests]\n");
    RUN_TEST(edf_util_test);

    printf("\n[Simulator Tests]\n");
    RUN_TEST(simulator_rms_basic);
    RUN_TEST(simulator_edf_basic);
    RUN_TEST(simulator_edf_overload);

    printf("\n[Utility Tests]\n");
    RUN_TEST(timer_basic);
    RUN_TEST(math_gcd_lcm);
    RUN_TEST(math_round4);

    printf("\n================================================================\n");
    printf("  Tests complete: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("================================================================\n\n");

    return g_tests_failed > 0 ? 1 : 0;
}
