#include "scheduler.h"

#include "prng.h"
#include "selection.h"

typedef enum {
    NON_PREEMPTIVE_FCFS,
    NON_PREEMPTIVE_SJF,
    NON_PREEMPTIVE_PRIORITY
} NonPreemptiveCriterion;

static int process_criterion(const Process *process,
                             NonPreemptiveCriterion criterion)
{
    if (criterion == NON_PREEMPTIVE_FCFS) {
        return process->arrival_time;
    }
    if (criterion == NON_PREEMPTIVE_SJF) {
        return process->burst_time;
    }
    return process->static_priority;
}

static bool collect_best_ready(const Simulation *simulation,
                               NonPreemptiveCriterion criterion,
                               IndexList *candidates,
                               SchedulerError *error)
{
    size_t index;
    bool found = false;
    int best_value = 0;

    index_list_clear(candidates);
    for (index = 0; index < simulation->processes.count; ++index) {
        const Process *process = &simulation->processes.items[index];
        int value;

        if (process->status != PROCESS_READY) {
            continue;
        }
        value = process_criterion(process, criterion);
        if (!found || value < best_value) {
            index_list_clear(candidates);
            best_value = value;
            found = true;
        }
        if (value == best_value && !index_list_append(candidates, index, error)) {
            return false;
        }
    }
    return true;
}

static bool scheduler_run_non_preemptive(const ProcessList *source,
                                         uint64_t seed,
                                         NonPreemptiveCriterion criterion,
                                         Simulation *result,
                                         SchedulerError *error)
{
    Simulation working;
    IndexList candidates;
    Prng prng;
    bool success = false;

    if (source == NULL || result == NULL) {
        error_set(error, EXIT_CODE_INTERNAL, "invalid scheduler request");
        return false;
    }
    process_list_init(&result->processes);
    timeline_init(&result->timeline);
    result->current_time = 0;
    result->completed_count = 0;
    result->current_process_index = SIMULATION_NO_PROCESS;
    index_list_init(&candidates);
    prng_init(&prng, seed);
    if (!simulation_init(&working, source, error)) {
        goto cleanup;
    }

    while (!simulation_is_complete(&working)) {
        size_t selected_index;

        if (!simulation_admit_current(&working, NULL, error)) {
            goto cleanup_working;
        }
        if (working.current_process_index == SIMULATION_NO_PROCESS) {
            if (!collect_best_ready(&working, criterion, &candidates, error)) {
                goto cleanup_working;
            }
            if (candidates.count > 0) {
                if (!selection_break_tie(&working, &candidates,
                                         working.current_process_index, &prng,
                                         &selected_index, error) ||
                    !simulation_dispatch(&working, selected_index, error)) {
                    goto cleanup_working;
                }
            } else if (!simulation_record_idle_second(&working, error)) {
                goto cleanup_working;
            }
        }
        if (working.current_process_index != SIMULATION_NO_PROCESS &&
            !simulation_execute_second(&working, error)) {
            goto cleanup_working;
        }
    }
    if (!simulation_validate(&working, error)) {
        goto cleanup_working;
    }
    *result = working;
    success = true;
    goto cleanup;

cleanup_working:
    simulation_destroy(&working);
cleanup:
    index_list_destroy(&candidates);
    return success;
}

bool scheduler_run_fcfs(const ProcessList *source, uint64_t seed,
                        Simulation *result, SchedulerError *error)
{
    return scheduler_run_non_preemptive(source, seed, NON_PREEMPTIVE_FCFS,
                                        result, error);
}

bool scheduler_run_sjf(const ProcessList *source, uint64_t seed,
                       Simulation *result, SchedulerError *error)
{
    return scheduler_run_non_preemptive(source, seed, NON_PREEMPTIVE_SJF,
                                        result, error);
}

bool scheduler_run_priority_non_preemptive(const ProcessList *source,
                                           uint64_t seed,
                                           Simulation *result,
                                           SchedulerError *error)
{
    return scheduler_run_non_preemptive(source, seed, NON_PREEMPTIVE_PRIORITY,
                                        result, error);
}
