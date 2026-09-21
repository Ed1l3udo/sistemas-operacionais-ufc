#ifndef SCHEDULER_INDEX_LIST_H
#define SCHEDULER_INDEX_LIST_H

#include <stdbool.h>
#include <stddef.h>

#include "error.h"

/* A dynamic sequence of process indices with no scheduling policy attached. */
typedef struct {
    size_t *items;
    size_t count;
    size_t capacity;
} IndexList;

void index_list_init(IndexList *list);
void index_list_clear(IndexList *list);
void index_list_destroy(IndexList *list);
bool index_list_append(IndexList *list, size_t value, SchedulerError *error);
const size_t *index_list_get(const IndexList *list, size_t index);

#endif
