#include "selection.h"

static bool candidate_is_valid(const Simulation *simulation, size_t index)
{
    return index < simulation->processes.count;
}

bool selection_break_tie(const Simulation *simulation,
                         const IndexList *candidates,
                         size_t current_process_index,
                         Prng *prng,
                         size_t *selected_index,
                         SchedulerError *error)
{
    size_t index;
    size_t minimum_remaining = 0;
    size_t matching_count = 0;
    size_t matching_offset;

    if (simulation == NULL || candidates == NULL || selected_index == NULL ||
        prng == NULL || candidates->count == 0) {
        error_set(error, EXIT_CODE_INTERNAL, "invalid tie-break request");
        return false;
    }
    if (current_process_index != SIMULATION_NO_PROCESS &&
        !candidate_is_valid(simulation, current_process_index)) {
        error_set(error, EXIT_CODE_INTERNAL, "invalid current process index");
        return false;
    }
    for (index = 0; index < candidates->count; ++index) {
        size_t candidate = candidates->items[index];
        size_t other;

        if (!candidate_is_valid(simulation, candidate)) {
            error_set(error, EXIT_CODE_INTERNAL, "invalid tie-break candidate");
            return false;
        }
        for (other = 0; other < index; ++other) {
            if (candidates->items[other] == candidate) {
                error_set(error, EXIT_CODE_INTERNAL,
                          "duplicate tie-break candidate");
                return false;
            }
        }
        if (candidate == current_process_index) {
            *selected_index = candidate;
            return true;
        }
    }

    for (index = 0; index < candidates->count; ++index) {
        const Process *process = process_list_get_const(
            &simulation->processes, candidates->items[index]);

        if (index == 0 || (size_t)process->remaining_time < minimum_remaining) {
            minimum_remaining = (size_t)process->remaining_time;
        }
    }
    for (index = 0; index < candidates->count; ++index) {
        const Process *process = process_list_get_const(
            &simulation->processes, candidates->items[index]);

        if ((size_t)process->remaining_time == minimum_remaining) {
            ++matching_count;
        }
    }
    if (matching_count == 1) {
        for (index = 0; index < candidates->count; ++index) {
            const Process *process = process_list_get_const(
                &simulation->processes, candidates->items[index]);

            if ((size_t)process->remaining_time == minimum_remaining) {
                *selected_index = candidates->items[index];
                return true;
            }
        }
    }
    if (!prng_next_bounded(prng, matching_count, &matching_offset)) {
        error_set(error, EXIT_CODE_INTERNAL, "unable to choose tied process");
        return false;
    }
    for (index = 0; index < candidates->count; ++index) {
        const Process *process = process_list_get_const(
            &simulation->processes, candidates->items[index]);

        if ((size_t)process->remaining_time == minimum_remaining) {
            if (matching_offset == 0) {
                *selected_index = candidates->items[index];
                return true;
            }
            --matching_offset;
        }
    }

    error_set(error, EXIT_CODE_INTERNAL, "tie-break selection failed");
    return false;
}
