#ifndef SCHEDULER_TIMELINE_H
#define SCHEDULER_TIMELINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error.h"

#define TIMELINE_IDLE_PROCESS_ID 0

typedef struct {
    int64_t start_time;
    int process_id;
} TimelineEntry;

typedef struct {
    TimelineEntry *items;
    size_t count;
    size_t capacity;
} Timeline;

void timeline_init(Timeline *timeline);
void timeline_destroy(Timeline *timeline);
bool timeline_append(Timeline *timeline, int64_t start_time, int process_id,
                     SchedulerError *error);
const TimelineEntry *timeline_get(const Timeline *timeline, size_t index);

#endif
