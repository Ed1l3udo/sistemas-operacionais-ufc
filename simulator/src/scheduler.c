#include "scheduler.h"

#include "prng.h"
#include "queue.h"
#include "selection.h"

typedef enum {
    NON_PREEMPTIVE_FCFS,
    NON_PREEMPTIVE_SJF,
    NON_PREEMPTIVE_PRIORITY
} NonPreemptiveCriterion;

typedef enum {
    PREEMPTIVE_SRTF,
    PREEMPTIVE_PRIORITY
} PreemptiveCriterion;

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

static int preemptive_criterion(const Process *process,
                                PreemptiveCriterion criterion)
{
    if (criterion == PREEMPTIVE_SRTF) {
        return process->remaining_time;
    }
    return process->static_priority;
}

static bool collect_best_preemptive(const Simulation *simulation,
                                    PreemptiveCriterion criterion,
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

        if (process->status != PROCESS_READY &&
            process->status != PROCESS_RUNNING) {
            continue;
        }
        value = preemptive_criterion(process, criterion);
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

static bool scheduler_run_preemptive(const ProcessList *source, uint64_t seed,
                                     PreemptiveCriterion criterion,
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

        if (!simulation_admit_current(&working, NULL, error) ||
            !collect_best_preemptive(&working, criterion, &candidates, error)) {
            goto cleanup_working;
        }
        if (candidates.count == 0) {
            if (!simulation_record_idle_second(&working, error)) {
                goto cleanup_working;
            }
            continue;
        }
        if (!selection_break_tie(&working, &candidates,
                                 working.current_process_index, &prng,
                                 &selected_index, error)) {
            goto cleanup_working;
        }
        if (selected_index != working.current_process_index) {
            if (working.current_process_index != SIMULATION_NO_PROCESS &&
                !simulation_preempt_current(&working, NULL, error)) {
                goto cleanup_working;
            }
            if (!simulation_dispatch(&working, selected_index, error)) {
                goto cleanup_working;
            }
        }
        if (!simulation_execute_second(&working, error)) {
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

bool scheduler_run_srtf(const ProcessList *source, uint64_t seed,
                        Simulation *result, SchedulerError *error)
{
    return scheduler_run_preemptive(source, seed, PREEMPTIVE_SRTF, result,
                                    error);
}

bool scheduler_run_priority_preemptive(const ProcessList *source,
                                       uint64_t seed,
                                       Simulation *result,
                                       SchedulerError *error)
{
    return scheduler_run_preemptive(source, seed, PREEMPTIVE_PRIORITY, result,
                                    error);
}

static bool validate_round_robin_input(const ProcessList *source, int quantum,
                                       int aging, Simulation *result,
                                       SchedulerError *error)
{
    size_t index;

    if (source == NULL || result == NULL || quantum <= 0 || aging < 0) {
        error_set(error, EXIT_CODE_INTERNAL, "invalid round-robin request");
        return false;
    }
    for (index = 0; index < source->count; ++index) {
        const Process *process = &source->items[index];

        if (process->arrival_time < 0 || process->burst_time <= 0 ||
            process->remaining_time <= 0 || process->static_priority <= 0 ||
            process->dynamic_priority <= 0) {
            error_set(error, EXIT_CODE_INTERNAL, "invalid source process");
            return false;
        }
    }
    return true;
}

static bool enqueue_admitted(Simulation *simulation, IndexList *admitted,
                             IndexQueue *ready_queue, SchedulerError *error)
{
    size_t index;

    if (!simulation_admit_current(simulation, admitted, error)) {
        return false;
    }
    for (index = 0; index < admitted->count; ++index) {
        size_t process_index = admitted->items[index];

        if (index_queue_contains(ready_queue, process_index) ||
            !index_queue_push(ready_queue, process_index, error)) {
            error_set(error, EXIT_CODE_INTERNAL,
                      "duplicate or invalid process in round-robin queue");
            return false;
        }
    }
    return true;
}

static bool dequeue_highest_dynamic_priority(const Simulation *simulation,
                                             IndexQueue *ready_queue,
                                             size_t *process_index,
                                             SchedulerError *error)
{
    size_t position;
    size_t selected_position = 0;
    bool found = false;
    int best_priority = 0;

    for (position = 0; position < index_queue_size(ready_queue); ++position) {
        size_t candidate_index;
        const Process *candidate;

        if (!index_queue_get(ready_queue, position, &candidate_index) ||
            candidate_index >= simulation->processes.count) {
            error_set(error, EXIT_CODE_INTERNAL,
                      "invalid process in round-robin queue");
            return false;
        }
        candidate = &simulation->processes.items[candidate_index];
        if (candidate->status != PROCESS_READY) {
            error_set(error, EXIT_CODE_INTERNAL,
                      "non-ready process in round-robin queue");
            return false;
        }
        if (!found || candidate->dynamic_priority < best_priority) {
            found = true;
            best_priority = candidate->dynamic_priority;
            selected_position = position;
        }
    }
    if (!found || !index_queue_remove_at(ready_queue, selected_position,
                                         process_index)) {
        error_set(error, EXIT_CODE_INTERNAL,
                  "unable to select round-robin process");
        return false;
    }
    return true;
}

static void apply_aging(Simulation *simulation, int aging)
{
    size_t index;

    if (aging == 0) {
        return;
    }
    for (index = 0; index < simulation->processes.count; ++index) {
        Process *process = &simulation->processes.items[index];

        if (process->status == PROCESS_READY) {
            if (process->dynamic_priority <= aging) {
                process->dynamic_priority = 1;
            } else {
                process->dynamic_priority -= aging;
            }
        }
    }
}

static bool scheduler_run_round_robin_common(const ProcessList *source,
                                             int quantum, int aging,
                                             bool use_priority,
                                             Simulation *result,
                                             SchedulerError *error)
{
    Simulation working;
    IndexQueue ready_queue;
    IndexList admitted;
    size_t pending_requeue = SIMULATION_NO_PROCESS;
    int quantum_used = 0;
    bool success = false;

    if (!validate_round_robin_input(source, quantum, aging, result, error)) {
        return false;
    }
    process_list_init(&result->processes);
    timeline_init(&result->timeline);
    result->current_time = 0;
    result->completed_count = 0;
    result->current_process_index = SIMULATION_NO_PROCESS;
    index_queue_init(&ready_queue);
    index_list_init(&admitted);
    if (!simulation_init(&working, source, error)) {
        goto cleanup;
    }

    while (!simulation_is_complete(&working)) {
        size_t process_index;

        if (!enqueue_admitted(&working, &admitted, &ready_queue, error)) {
            goto cleanup_working;
        }
        if (pending_requeue != SIMULATION_NO_PROCESS) {
            if (index_queue_contains(&ready_queue, pending_requeue) ||
                !index_queue_push(&ready_queue, pending_requeue, error)) {
                error_set(error, EXIT_CODE_INTERNAL,
                          "unable to requeue expired process");
                goto cleanup_working;
            }
            pending_requeue = SIMULATION_NO_PROCESS;
        }
        if (working.current_process_index == SIMULATION_NO_PROCESS) {
            if (index_queue_is_empty(&ready_queue)) {
                if (!simulation_record_idle_second(&working, error)) {
                    goto cleanup_working;
                }
                continue;
            }
            if (use_priority) {
                if (!dequeue_highest_dynamic_priority(&working, &ready_queue,
                                                     &process_index, error)) {
                    goto cleanup_working;
                }
            } else if (!index_queue_pop(&ready_queue, &process_index)) {
                error_set(error, EXIT_CODE_INTERNAL,
                          "unable to dequeue round-robin process");
                goto cleanup_working;
            }
            if (!simulation_dispatch(&working, process_index, error)) {
                goto cleanup_working;
            }
            quantum_used = 0;
        }
        if (!simulation_execute_second(&working, error)) {
            goto cleanup_working;
        }
        ++quantum_used;
        if (quantum_used == quantum) {
            if (use_priority) {
                apply_aging(&working, aging);
            }
            if (working.current_process_index != SIMULATION_NO_PROCESS) {
                if (!simulation_preempt_current(&working, &pending_requeue,
                                                error)) {
                    goto cleanup_working;
                }
            }
            quantum_used = 0;
        }
    }
    if (!index_queue_is_empty(&ready_queue) ||
        pending_requeue != SIMULATION_NO_PROCESS || !simulation_validate(&working, error)) {
        if (error->code == EXIT_CODE_SUCCESS) {
            error_set(error, EXIT_CODE_INTERNAL, "round-robin final state is invalid");
        }
        goto cleanup_working;
    }
    *result = working;
    success = true;
    goto cleanup;

cleanup_working:
    simulation_destroy(&working);
cleanup:
    index_list_destroy(&admitted);
    index_queue_destroy(&ready_queue);
    return success;
}

bool scheduler_run_round_robin(const ProcessList *source, int quantum,
                               Simulation *result, SchedulerError *error)
{
    return scheduler_run_round_robin_common(source, quantum, 0, false, result,
                                            error);
}

bool scheduler_run_priority_round_robin(const ProcessList *source, int quantum,
                                        int aging, Simulation *result,
                                        SchedulerError *error)
{
    return scheduler_run_round_robin_common(source, quantum, aging, true, result,
                                            error);
}
