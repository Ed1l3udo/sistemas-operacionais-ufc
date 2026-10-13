#ifndef SCHEDULER_JSON_OUTPUT_H
#define SCHEDULER_JSON_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "cli.h"
#include "config.h"
#include "metrics.h"

/* Writes a complete JSON string, including quotes and control-character escapes. */
bool json_output_write_escaped_string(FILE *stream, const char *text,
                                      SchedulerError *error);
bool json_output_begin_document(FILE *stream, uint64_t seed,
                                const Config *config, SchedulerError *error);
bool json_output_write_algorithm(FILE *stream, AlgorithmType algorithm,
                                 const Simulation *simulation,
                                 const MetricsReport *report, bool prepend_comma,
                                 SchedulerError *error);
bool json_output_end_document(FILE *stream, SchedulerError *error);

#endif
