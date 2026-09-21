#include "timeline.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#define TIMELINE_INITIAL_CAPACITY 16

static bool timeline_reserve(Timeline *timeline, size_t required,
                             SchedulerError *error)
{
    TimelineEntry *items;
    size_t capacity;

    if (required <= timeline->capacity) {
        return true;
    }
    if (required > SIZE_MAX / sizeof(*timeline->items)) {
        error_set(error, EXIT_CODE_INTERNAL, "timeline is too large to allocate");
        return false;
    }
    capacity = timeline->capacity == 0 ? TIMELINE_INITIAL_CAPACITY
                                        : timeline->capacity;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2) {
            capacity = required;
            break;
        }
        capacity *= 2;
    }
    items = realloc(timeline->items, capacity * sizeof(*timeline->items));
    if (items == NULL) {
        error_set(error, EXIT_CODE_INTERNAL, "unable to allocate timeline memory");
        return false;
    }
    timeline->items = items;
    timeline->capacity = capacity;
    return true;
}

void timeline_init(Timeline *timeline)
{
    if (timeline == NULL) {
        return;
    }
    timeline->items = NULL;
    timeline->count = 0;
    timeline->capacity = 0;
}

void timeline_destroy(Timeline *timeline)
{
    if (timeline == NULL) {
        return;
    }
    free(timeline->items);
    timeline_init(timeline);
}

bool timeline_append(Timeline *timeline, int64_t start_time, int process_id,
                     SchedulerError *error)
{
    int64_t expected_start;

    if (timeline == NULL || start_time < 0 || start_time == INT64_MAX ||
        process_id < TIMELINE_IDLE_PROCESS_ID) {
        error_set(error, EXIT_CODE_INTERNAL, "invalid timeline entry");
        return false;
    }
    expected_start = 0;
    if (timeline->count > 0) {
        expected_start = timeline->items[timeline->count - 1].start_time + 1;
    }
    if (start_time != expected_start) {
        error_set(error, EXIT_CODE_INTERNAL,
                  "timeline entries must be contiguous and chronological");
        return false;
    }
    if (!timeline_reserve(timeline, timeline->count + 1, error)) {
        return false;
    }
    timeline->items[timeline->count].start_time = start_time;
    timeline->items[timeline->count].process_id = process_id;
    ++timeline->count;
    return true;
}

const TimelineEntry *timeline_get(const Timeline *timeline, size_t index)
{
    if (timeline == NULL || index >= timeline->count) {
        return NULL;
    }
    return &timeline->items[index];
}
