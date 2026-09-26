#ifndef SCHEDULER_SCHEDULER_H
#define SCHEDULER_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

#include "simulation.h"

bool scheduler_run_fcfs(const ProcessList *source, uint64_t seed,
                        Simulation *result, SchedulerError *error);
bool scheduler_run_sjf(const ProcessList *source, uint64_t seed,
                       Simulation *result, SchedulerError *error);
bool scheduler_run_priority_non_preemptive(const ProcessList *source,
                                           uint64_t seed,
                                           Simulation *result,
                                           SchedulerError *error);

#endif
