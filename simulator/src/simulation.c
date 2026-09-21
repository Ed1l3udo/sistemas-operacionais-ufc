#include "simulation.h"

#include <limits.h>

static bool simulation_can_advance(const Simulation *simulation,
                                   SchedulerError *error)
{
    if (simulation->current_time == INT64_MAX) {
        error_set(error, EXIT_CODE_INTERNAL, "simulation clock overflow");
        return false;
    }
    return true;
}

bool simulation_init(Simulation *simulation, const ProcessList *source,
                     SchedulerError *error)
{
    if (simulation == NULL || source == NULL) {
        error_set(error, EXIT_CODE_INTERNAL, "invalid simulation initialization");
        return false;
    }
    process_list_init(&simulation->processes);
    timeline_init(&simulation->timeline);
    simulation->current_time = 0;
    simulation->completed_count = 0;
    simulation->current_process_index = SIMULATION_NO_PROCESS;
    return process_list_clone(source, &simulation->processes, error);
}

void simulation_destroy(Simulation *simulation)
{
    if (simulation == NULL) {
        return;
    }
    timeline_destroy(&simulation->timeline);
    process_list_destroy(&simulation->processes);
    simulation->current_time = 0;
    simulation->completed_count = 0;
    simulation->current_process_index = SIMULATION_NO_PROCESS;
}

Process *simulation_current_process(Simulation *simulation)
{
    if (simulation == NULL ||
        simulation->current_process_index == SIMULATION_NO_PROCESS) {
        return NULL;
    }
    return process_list_get(&simulation->processes,
                            simulation->current_process_index);
}

const Process *simulation_current_process_const(const Simulation *simulation)
{
    if (simulation == NULL ||
        simulation->current_process_index == SIMULATION_NO_PROCESS) {
        return NULL;
    }
    return process_list_get_const(&simulation->processes,
                                  simulation->current_process_index);
}

bool simulation_is_complete(const Simulation *simulation)
{
    return simulation != NULL &&
           simulation->completed_count == simulation->processes.count;
}

bool simulation_has_ready_process(const Simulation *simulation)
{
    size_t index;

    if (simulation == NULL) {
        return false;
    }
    for (index = 0; index < simulation->processes.count; ++index) {
        if (simulation->processes.items[index].status == PROCESS_READY) {
            return true;
        }
    }
    return false;
}

bool simulation_next_arrival(const Simulation *simulation, int64_t *arrival_time)
{
    size_t index;
    bool found = false;
    int64_t next_arrival = 0;

    if (simulation == NULL || arrival_time == NULL) {
        return false;
    }
    for (index = 0; index < simulation->processes.count; ++index) {
        const Process *process = &simulation->processes.items[index];

        if (process->status == PROCESS_NEW &&
            (!found || process->arrival_time < next_arrival)) {
            next_arrival = process->arrival_time;
            found = true;
        }
    }
    if (found) {
        *arrival_time = next_arrival;
    }
    return found;
}

bool simulation_admit_current(Simulation *simulation, IndexList *admitted,
                              SchedulerError *error)
{
    size_t index;

    if (simulation == NULL) {
        error_set(error, EXIT_CODE_INTERNAL, "simulation is not initialized");
        return false;
    }
    if (admitted != NULL) {
        index_list_clear(admitted);
    }
    for (index = 0; index < simulation->processes.count; ++index) {
        Process *process = &simulation->processes.items[index];

        if (process->status == PROCESS_NEW &&
            process->arrival_time <= simulation->current_time) {
            process->status = PROCESS_READY;
            if (admitted != NULL && !index_list_append(admitted, index, error)) {
                return false;
            }
        }
    }
    return true;
}

bool simulation_dispatch(Simulation *simulation, size_t process_index,
                         SchedulerError *error)
{
    Process *process;

    if (simulation == NULL || process_index >= simulation->processes.count) {
        error_set(error, EXIT_CODE_INTERNAL, "invalid process index for dispatch");
        return false;
    }
    if (simulation->current_process_index != SIMULATION_NO_PROCESS) {
        error_set(error, EXIT_CODE_INTERNAL, "CPU is already occupied");
        return false;
    }
    process = &simulation->processes.items[process_index];
    if (process->status != PROCESS_READY) {
        error_set(error, EXIT_CODE_INTERNAL,
                  "only a ready process may be dispatched");
        return false;
    }
    process->status = PROCESS_RUNNING;
    simulation->current_process_index = process_index;
    return true;
}

bool simulation_execute_second(Simulation *simulation, SchedulerError *error)
{
    Process *process;

    if (simulation == NULL ||
        simulation->current_process_index == SIMULATION_NO_PROCESS) {
        error_set(error, EXIT_CODE_INTERNAL, "CPU has no running process");
        return false;
    }
    process = simulation_current_process(simulation);
    if (process == NULL || process->status != PROCESS_RUNNING ||
        process->remaining_time <= 0) {
        error_set(error, EXIT_CODE_INTERNAL, "invalid running process state");
        return false;
    }
    if (!simulation_can_advance(simulation, error) ||
        !timeline_append(&simulation->timeline, simulation->current_time,
                         process->id, error)) {
        return false;
    }
    if (process->first_execution == -1) {
        process->first_execution = simulation->current_time;
    }
    --process->remaining_time;
    ++simulation->current_time;
    if (process->remaining_time == 0) {
        process->completion_time = simulation->current_time;
        process->status = PROCESS_FINISHED;
        ++simulation->completed_count;
        simulation->current_process_index = SIMULATION_NO_PROCESS;
    }
    return true;
}

bool simulation_record_idle_second(Simulation *simulation, SchedulerError *error)
{
    if (simulation == NULL) {
        error_set(error, EXIT_CODE_INTERNAL, "simulation is not initialized");
        return false;
    }
    if (simulation->current_process_index != SIMULATION_NO_PROCESS) {
        error_set(error, EXIT_CODE_INTERNAL,
                  "cannot record idle time while CPU is occupied");
        return false;
    }
    if (simulation_has_ready_process(simulation)) {
        error_set(error, EXIT_CODE_INTERNAL,
                  "cannot record idle time while a process is ready");
        return false;
    }
    if (!simulation_can_advance(simulation, error) ||
        !timeline_append(&simulation->timeline, simulation->current_time,
                         TIMELINE_IDLE_PROCESS_ID, error)) {
        return false;
    }
    ++simulation->current_time;
    return true;
}

bool simulation_preempt_current(Simulation *simulation, size_t *preempted_index,
                                SchedulerError *error)
{
    Process *process;
    size_t index;

    if (simulation == NULL ||
        simulation->current_process_index == SIMULATION_NO_PROCESS) {
        error_set(error, EXIT_CODE_INTERNAL, "CPU has no running process to preempt");
        return false;
    }
    index = simulation->current_process_index;
    process = simulation_current_process(simulation);
    if (process == NULL || process->status != PROCESS_RUNNING) {
        error_set(error, EXIT_CODE_INTERNAL, "invalid running process state");
        return false;
    }
    process->status = PROCESS_READY;
    simulation->current_process_index = SIMULATION_NO_PROCESS;
    if (preempted_index != NULL) {
        *preempted_index = index;
    }
    return true;
}

bool simulation_validate(const Simulation *simulation, SchedulerError *error)
{
    size_t index;
    size_t finished_count = 0;
    size_t running_count = 0;

    if (simulation == NULL || simulation->current_time < 0 ||
        simulation->timeline.count != (size_t)simulation->current_time) {
        error_set(error, EXIT_CODE_INTERNAL, "simulation clock and timeline mismatch");
        return false;
    }
    for (index = 0; index < simulation->processes.count; ++index) {
        const Process *process = &simulation->processes.items[index];

        if (process->static_priority <= 0 || process->dynamic_priority <= 0) {
            error_set(error, EXIT_CODE_INTERNAL, "process priority invariant failed");
            return false;
        }
        if (process->status == PROCESS_NEW) {
            if ((int64_t)process->arrival_time < simulation->current_time ||
                process->remaining_time != process->burst_time) {
                error_set(error, EXIT_CODE_INTERNAL, "new process invariant failed");
                return false;
            }
        } else if (process->status == PROCESS_READY) {
            if (process->arrival_time > simulation->current_time ||
                process->remaining_time <= 0) {
                error_set(error, EXIT_CODE_INTERNAL, "ready process invariant failed");
                return false;
            }
        } else if (process->status == PROCESS_RUNNING) {
            ++running_count;
            if (index != simulation->current_process_index ||
                process->remaining_time <= 0) {
                error_set(error, EXIT_CODE_INTERNAL, "running process invariant failed");
                return false;
            }
        } else if (process->status == PROCESS_FINISHED) {
            ++finished_count;
            if (process->remaining_time != 0 || process->completion_time < 0) {
                error_set(error, EXIT_CODE_INTERNAL, "finished process invariant failed");
                return false;
            }
        } else {
            error_set(error, EXIT_CODE_INTERNAL, "unknown process state");
            return false;
        }
    }
    if (running_count > 1 ||
        (simulation->current_process_index == SIMULATION_NO_PROCESS &&
         running_count != 0) ||
        (simulation->current_process_index != SIMULATION_NO_PROCESS &&
         running_count != 1) ||
        finished_count != simulation->completed_count) {
        error_set(error, EXIT_CODE_INTERNAL, "simulation process-state invariant failed");
        return false;
    }
    return true;
}
