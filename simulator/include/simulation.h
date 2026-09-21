#ifndef SCHEDULER_SIMULATION_H
#define SCHEDULER_SIMULATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "index_list.h"
#include "process.h"
#include "timeline.h"

#define SIMULATION_NO_PROCESS SIZE_MAX

typedef struct {
    ProcessList processes;
    int64_t current_time;
    size_t completed_count;
    size_t current_process_index;
    Timeline timeline;
} Simulation;

bool simulation_init(Simulation *simulation, const ProcessList *source,
                     SchedulerError *error);
void simulation_destroy(Simulation *simulation);
Process *simulation_current_process(Simulation *simulation);
const Process *simulation_current_process_const(const Simulation *simulation);
bool simulation_is_complete(const Simulation *simulation);
bool simulation_has_ready_process(const Simulation *simulation);
bool simulation_next_arrival(const Simulation *simulation, int64_t *arrival_time);
bool simulation_admit_current(Simulation *simulation, IndexList *admitted,
                              SchedulerError *error);
bool simulation_dispatch(Simulation *simulation, size_t process_index,
                         SchedulerError *error);
bool simulation_execute_second(Simulation *simulation, SchedulerError *error);
bool simulation_record_idle_second(Simulation *simulation, SchedulerError *error);
bool simulation_preempt_current(Simulation *simulation, size_t *preempted_index,
                                SchedulerError *error);
bool simulation_validate(const Simulation *simulation, SchedulerError *error);

#endif
