#ifndef SCHEDULER_PROCESS_H
#define SCHEDULER_PROCESS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "error.h"

typedef enum {
    PROCESS_NEW,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_FINISHED
} ProcessStatus;

typedef struct {
    int id;
    int arrival_time;
    int burst_time;
    int remaining_time;
    int static_priority;
    int dynamic_priority;
    int64_t first_execution;
    int64_t completion_time;
    ProcessStatus status;
} Process;

typedef struct {
    Process *items;
    size_t count;
    size_t capacity;
} ProcessList;

void process_list_init(ProcessList *list);
bool process_list_append(ProcessList *list, int arrival_time, int burst_time,
                         int static_priority, SchedulerError *error);
bool process_list_clone(const ProcessList *source, ProcessList *destination,
                        SchedulerError *error);
void process_list_destroy(ProcessList *list);
Process *process_list_get(ProcessList *list, size_t index);
const Process *process_list_get_const(const ProcessList *list, size_t index);
bool process_list_read(FILE *input, ProcessList *list, SchedulerError *error);

#endif
