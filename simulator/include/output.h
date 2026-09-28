#ifndef SCHEDULER_OUTPUT_H
#define SCHEDULER_OUTPUT_H

#include <stdbool.h>
#include <stdio.h>

#include "cli.h"
#include "config.h"
#include "metrics.h"

const char *output_algorithm_name(AlgorithmType algorithm);
bool output_print_report(FILE *stream, AlgorithmType algorithm,
                         const Config *config, const Simulation *simulation,
                         const MetricsReport *report, SchedulerError *error);

#endif
