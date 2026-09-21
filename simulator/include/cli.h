#ifndef SCHEDULER_CLI_H
#define SCHEDULER_CLI_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "error.h"

typedef enum {
    ALGORITHM_ALL,
    ALGORITHM_FCFS,
    ALGORITHM_SJF,
    ALGORITHM_SRTF,
    ALGORITHM_PRIORITY_NON_PREEMPTIVE,
    ALGORITHM_PRIORITY_PREEMPTIVE,
    ALGORITHM_RR,
    ALGORITHM_PRIORITY_RR
} AlgorithmType;

typedef struct {
    const char *config_path;
    AlgorithmType algorithm;
    bool seed_provided;
    uint64_t seed;
} CliOptions;

typedef enum {
    CLI_PARSE_OK,
    CLI_PARSE_HELP,
    CLI_PARSE_ERROR
} CliParseResult;

CliParseResult cli_parse(int argc, char *argv[], CliOptions *options,
                         SchedulerError *error);
void cli_print_usage(FILE *stream, const char *program_name);

#endif
