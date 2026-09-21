#include "index_list.h"

#include <stdint.h>
#include <stdlib.h>

#define INDEX_LIST_INITIAL_CAPACITY 8

static bool index_list_reserve(IndexList *list, size_t required,
                               SchedulerError *error)
{
    size_t *items;
    size_t capacity;

    if (required <= list->capacity) {
        return true;
    }
    if (required > SIZE_MAX / sizeof(*list->items)) {
        error_set(error, EXIT_CODE_INTERNAL, "index list is too large to allocate");
        return false;
    }
    capacity = list->capacity == 0 ? INDEX_LIST_INITIAL_CAPACITY : list->capacity;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2) {
            capacity = required;
            break;
        }
        capacity *= 2;
    }
    items = realloc(list->items, capacity * sizeof(*list->items));
    if (items == NULL) {
        error_set(error, EXIT_CODE_INTERNAL, "unable to allocate index list memory");
        return false;
    }
    list->items = items;
    list->capacity = capacity;
    return true;
}

void index_list_init(IndexList *list)
{
    if (list == NULL) {
        return;
    }
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void index_list_clear(IndexList *list)
{
    if (list != NULL) {
        list->count = 0;
    }
}

void index_list_destroy(IndexList *list)
{
    if (list == NULL) {
        return;
    }
    free(list->items);
    index_list_init(list);
}

bool index_list_append(IndexList *list, size_t value, SchedulerError *error)
{
    if (list == NULL) {
        error_set(error, EXIT_CODE_INTERNAL, "index list is not initialized");
        return false;
    }
    if (list->count == SIZE_MAX) {
        error_set(error, EXIT_CODE_INTERNAL, "index list has reached its size limit");
        return false;
    }
    if (!index_list_reserve(list, list->count + 1, error)) {
        return false;
    }
    list->items[list->count] = value;
    ++list->count;
    return true;
}

const size_t *index_list_get(const IndexList *list, size_t index)
{
    if (list == NULL || index >= list->count) {
        return NULL;
    }
    return &list->items[index];
}
