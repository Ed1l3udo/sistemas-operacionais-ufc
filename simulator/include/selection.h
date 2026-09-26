#ifndef SCHEDULER_SELECTION_H
#define SCHEDULER_SELECTION_H

#include <stdbool.h>
#include <stddef.h>

#include "prng.h"
#include "simulation.h"

bool selection_break_tie(const Simulation *simulation,
                         const IndexList *candidates,
                         size_t current_process_index,
                         Prng *prng,
                         size_t *selected_index,
                         SchedulerError *error);

#endif
