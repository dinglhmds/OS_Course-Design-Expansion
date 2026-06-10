/**
 * @file analysis.c
 * @brief Schedulability analysis implementation
 */

#include "scheduler.h"
#include "task.h"
#include <string.h>

SchedulabilityResult schedulability_analyze(const TaskSet* ts, SchedulerType type) {
    SchedulabilityResult result;
    memset(&result, 0, sizeof(result));

    result.utilization = taskset_total_utilization(ts);

    switch (type) {
        case SCHED_RMS: {
            result.bound = taskset_rms_bound(ts);

            bool ll = rms_ll_test(ts);
            if (ll) {
                result.schedulable = true;
                strncpy(result.method_used, "Liu&Layland bound (sufficient)", sizeof(result.method_used) - 1);
                snprintf(result.details, sizeof(result.details),
                         "U=%.4f <= bound=%.4f", result.utilization, result.bound);
                return result;
            }

            bool hyp = rms_hyperbolic_test(ts);
            if (hyp) {
                result.schedulable = true;
                strncpy(result.method_used, "Hyperbolic bound (sufficient)", sizeof(result.method_used) - 1);
                return result;
            }

            bool rta = rms_response_time_analysis(ts, NULL);
            result.schedulable = rta;
            strncpy(result.method_used, "Response Time Analysis (exact)", sizeof(result.method_used) - 1);
            return result;
        }

        case SCHED_EDF: {
            bool all_implicit = true;
            for (int i = 0; i < ts->count; i++) {
                if (fabs(ts->tasks[i].deadline - ts->tasks[i].period) > EPSILON) {
                    all_implicit = false;
                    break;
                }
            }

            if (all_implicit) {
                bool util = edf_utilization_test(ts);
                result.schedulable = util;
                result.bound = 1.0;
                strncpy(result.method_used,
                        util ? "Utilization test (necessary & sufficient)"
                             : "Utilization test (necessary & sufficient)",
                        sizeof(result.method_used) - 1);
                return result;
            } else {
                bool dens = edf_density_test(ts);
                if (dens) {
                    result.schedulable = true;
                    strncpy(result.method_used, "Density test (sufficient)", sizeof(result.method_used) - 1);
                    return result;
                }

                bool pdc = edf_processor_demand_test(ts);
                result.schedulable = pdc;
                strncpy(result.method_used, "Processor Demand (necessary & sufficient)", sizeof(result.method_used) - 1);
                return result;
            }
        }

        case SCHED_LLF: {
            bool util = edf_utilization_test(ts);
            result.schedulable = util;
            result.bound = 1.0;
            strncpy(result.method_used, "Same as EDF (utilization)", sizeof(result.method_used) - 1);
            return result;
        }

        case SCHED_DMS: {
            bool rta = dms_response_time_analysis(ts, NULL);
            result.schedulable = rta;
            strncpy(result.method_used, "Response Time Analysis (exact)", sizeof(result.method_used) - 1);
            return result;
        }

        case SCHED_FCFS:
        case SCHED_SJF:
        default: {
            result.schedulable = false;
            strncpy(result.method_used, "No schedulability test for non-real-time scheduler", sizeof(result.method_used) - 1);
            return result;
        }
    }
}
