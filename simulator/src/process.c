#include "process.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PROCESS_LINE_CAPACITY 1024
#define PROCESS_LIST_INITIAL_CAPACITY 8

static char *skip_whitespace(char *text)
{
    while (*text != '\0' && isspace((unsigned char)*text)) {
        ++text;
    }

    return text;
}

static bool line_is_blank(const char *line)
{
    while (*line != '\0') {
        if (!isspace((unsigned char)*line)) {
            return false;
        }
        ++line;
    }

    return true;
}

static bool parse_int_token(char **cursor, int *value, size_t line_number,
                            SchedulerError *error)
{
    char *end = NULL;
    long parsed_value;

    *cursor = skip_whitespace(*cursor);
    if (**cursor == '\0') {
        error_set(error, EXIT_CODE_INPUT,
                  "process input line %zu: expected an integer", line_number);
        return false;
    }

    errno = 0;
    parsed_value = strtol(*cursor, &end, 10);
    if (end == *cursor) {
        error_set(error, EXIT_CODE_INPUT,
                  "process input line %zu: invalid integer", line_number);
        return false;
    }
    if (errno == ERANGE || parsed_value < INT_MIN || parsed_value > INT_MAX) {
        error_set(error, EXIT_CODE_INPUT,
                  "process input line %zu: integer is outside the int range",
                  line_number);
        return false;
    }

    *value = (int)parsed_value;
    *cursor = end;
    return true;
}

static bool discard_overlong_line(FILE *input)
{
    int character;

    do {
        character = fgetc(input);
    } while (character != '\n' && character != EOF);

    return character != EOF || !ferror(input);
}

static bool process_list_reserve(ProcessList *list, size_t required_capacity,
                                 SchedulerError *error)
{
    Process *resized_items;
    size_t new_capacity;

    if (required_capacity <= list->capacity) {
        return true;
    }
    if (required_capacity > SIZE_MAX / sizeof(*list->items)) {
        error_set(error, EXIT_CODE_INTERNAL,
                  "process list is too large to allocate");
        return false;
    }

    new_capacity = list->capacity == 0 ? PROCESS_LIST_INITIAL_CAPACITY
                                        : list->capacity;
    while (new_capacity < required_capacity) {
        if (new_capacity > SIZE_MAX / 2) {
            new_capacity = required_capacity;
            break;
        }
        new_capacity *= 2;
    }

    resized_items = realloc(list->items, new_capacity * sizeof(*list->items));
    if (resized_items == NULL) {
        error_set(error, EXIT_CODE_INTERNAL,
                  "unable to allocate memory for processes");
        return false;
    }

    list->items = resized_items;
    list->capacity = new_capacity;
    return true;
}

void process_list_init(ProcessList *list)
{
    if (list == NULL) {
        return;
    }

    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

bool process_list_append(ProcessList *list, int arrival_time, int burst_time,
                         int static_priority, SchedulerError *error)
{
    Process *process;

    if (list == NULL) {
        error_set(error, EXIT_CODE_INTERNAL, "process list is not initialized");
        return false;
    }
    if (arrival_time < 0 || burst_time <= 0 || static_priority <= 0) {
        error_set(error, EXIT_CODE_INPUT, "invalid process values");
        return false;
    }
    if (list->count >= (size_t)INT_MAX) {
        error_set(error, EXIT_CODE_INTERNAL,
                  "too many processes to assign an int identifier");
        return false;
    }
    if (!process_list_reserve(list, list->count + 1, error)) {
        return false;
    }

    process = &list->items[list->count];
    process->id = (int)list->count + 1;
    process->arrival_time = arrival_time;
    process->burst_time = burst_time;
    process->remaining_time = burst_time;
    process->static_priority = static_priority;
    process->dynamic_priority = static_priority;
    process->first_execution = -1;
    process->completion_time = -1;
    process->status = PROCESS_NEW;
    ++list->count;
    return true;
}

bool process_list_clone(const ProcessList *source, ProcessList *destination,
                        SchedulerError *error)
{
    ProcessList clone;

    if (source == NULL || destination == NULL || source == destination) {
        error_set(error, EXIT_CODE_INTERNAL, "invalid process list copy request");
        return false;
    }

    process_list_init(&clone);
    if (source->count > 0) {
        if (!process_list_reserve(&clone, source->count, error)) {
            return false;
        }
        memcpy(clone.items, source->items, source->count * sizeof(*source->items));
        clone.count = source->count;
    }

    process_list_destroy(destination);
    *destination = clone;
    return true;
}

void process_list_destroy(ProcessList *list)
{
    if (list == NULL) {
        return;
    }

    free(list->items);
    process_list_init(list);
}

Process *process_list_get(ProcessList *list, size_t index)
{
    if (list == NULL || index >= list->count) {
        return NULL;
    }

    return &list->items[index];
}

const Process *process_list_get_const(const ProcessList *list, size_t index)
{
    if (list == NULL || index >= list->count) {
        return NULL;
    }

    return &list->items[index];
}

bool process_list_read(FILE *input, ProcessList *list, SchedulerError *error)
{
    char line[PROCESS_LINE_CAPACITY];
    size_t line_number = 0;

    if (input == NULL || list == NULL) {
        error_set(error, EXIT_CODE_INTERNAL, "invalid process input stream");
        return false;
    }

    while (fgets(line, sizeof(line), input) != NULL) {
        char *cursor;
        int arrival_time;
        int burst_time;
        int static_priority;

        ++line_number;
        if (strchr(line, '\n') == NULL && !feof(input)) {
            (void)discard_overlong_line(input);
            error_set(error, EXIT_CODE_INPUT,
                      "process input line %zu exceeds %d characters", line_number,
                      PROCESS_LINE_CAPACITY - 1);
            return false;
        }
        if (line_is_blank(line)) {
            continue;
        }

        cursor = line;
        if (!parse_int_token(&cursor, &arrival_time, line_number, error) ||
            !parse_int_token(&cursor, &burst_time, line_number, error) ||
            !parse_int_token(&cursor, &static_priority, line_number, error)) {
            return false;
        }
        cursor = skip_whitespace(cursor);
        if (*cursor != '\0') {
            error_set(error, EXIT_CODE_INPUT,
                      "process input line %zu: expected exactly three integers",
                      line_number);
            return false;
        }
        if (arrival_time < 0) {
            error_set(error, EXIT_CODE_INPUT,
                      "process input line %zu: arrival time must be non-negative",
                      line_number);
            return false;
        }
        if (burst_time <= 0) {
            error_set(error, EXIT_CODE_INPUT,
                      "process input line %zu: burst time must be greater than zero",
                      line_number);
            return false;
        }
        if (static_priority <= 0) {
            error_set(error, EXIT_CODE_INPUT,
                      "process input line %zu: priority must be greater than zero",
                      line_number);
            return false;
        }
        if (!process_list_append(list, arrival_time, burst_time, static_priority,
                                 error)) {
            return false;
        }
    }

    if (ferror(input)) {
        error_set(error, EXIT_CODE_INPUT, "unable to read process input");
        return false;
    }
    if (list->count == 0) {
        error_set(error, EXIT_CODE_INPUT, "process input contains no processes");
        return false;
    }

    return true;
}
