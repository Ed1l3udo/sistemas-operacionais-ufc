#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define SCHEDULER_MAX_PROCESSES 10000
#define SCHEDULER_ERROR_SIZE 256

typedef struct {
    int id;
    int arrival;
    int burst;
    int priority;
} ProcessSpec;

typedef struct {
    int quantum;
    int aging;
    uint32_t seed;
} SchedulerConfig;

typedef enum {
    ALG_FCFS,
    ALG_SJF,
    ALG_SRTF,
    ALG_PRIORITY_NP,
    ALG_PRIORITY_P,
    ALG_RR,
    ALG_PRIORITY_RR,
    ALG_COUNT
} Algorithm;

typedef struct {
    int first_dispatch;
    int completion;
    int turnaround;
    int waiting;
    int response;
} ProcessMetrics;

typedef struct {
    Algorithm algorithm;
    ProcessMetrics *metrics;
    int *timeline;
    size_t timeline_length;
    double average_turnaround;
    double average_waiting;
    double average_response;
    int context_switches;
} SimulationResult;

const char *algorithm_key(Algorithm algorithm);
const char *algorithm_label(Algorithm algorithm);
bool parse_algorithm(const char *text, Algorithm *algorithm);

bool read_config(const char *path, SchedulerConfig *config, char *error, size_t error_size);
bool read_processes(FILE *stream, ProcessSpec **processes, size_t *count,
                    char *error, size_t error_size);

bool simulate(const ProcessSpec *processes, size_t count, const SchedulerConfig *config,
              Algorithm algorithm, SimulationResult *result,
              char *error, size_t error_size);
void free_result(SimulationResult *result);

void print_text_result(FILE *stream, const ProcessSpec *processes, size_t count,
                       const SimulationResult *result);
void print_json(FILE *stream, const ProcessSpec *processes, size_t count,
                const SchedulerConfig *config, const SimulationResult *results,
                size_t result_count);

#endif

