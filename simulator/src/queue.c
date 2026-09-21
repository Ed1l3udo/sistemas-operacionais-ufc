#include "queue.h"

#include <stdint.h>
#include <stdlib.h>

#define INDEX_QUEUE_INITIAL_CAPACITY 8

static bool index_queue_grow(IndexQueue *queue, SchedulerError *error)
{
    size_t *items;
    size_t capacity;
    size_t index;

    if (queue->count < queue->capacity) {
        return true;
    }
    if (queue->capacity > SIZE_MAX / 2 ||
        queue->capacity > SIZE_MAX / sizeof(*queue->items)) {
        error_set(error, EXIT_CODE_INTERNAL, "index queue is too large to grow");
        return false;
    }
    capacity = queue->capacity == 0 ? INDEX_QUEUE_INITIAL_CAPACITY
                                     : queue->capacity * 2;
    if (capacity > SIZE_MAX / sizeof(*queue->items)) {
        error_set(error, EXIT_CODE_INTERNAL, "index queue is too large to allocate");
        return false;
    }
    items = malloc(capacity * sizeof(*items));
    if (items == NULL) {
        error_set(error, EXIT_CODE_INTERNAL, "unable to allocate index queue memory");
        return false;
    }
    for (index = 0; index < queue->count; ++index) {
        items[index] = queue->items[(queue->head + index) % queue->capacity];
    }
    free(queue->items);
    queue->items = items;
    queue->capacity = capacity;
    queue->head = 0;
    return true;
}

void index_queue_init(IndexQueue *queue)
{
    if (queue == NULL) {
        return;
    }
    queue->items = NULL;
    queue->capacity = 0;
    queue->count = 0;
    queue->head = 0;
}

void index_queue_destroy(IndexQueue *queue)
{
    if (queue == NULL) {
        return;
    }
    free(queue->items);
    index_queue_init(queue);
}

bool index_queue_is_empty(const IndexQueue *queue)
{
    return queue == NULL || queue->count == 0;
}

size_t index_queue_size(const IndexQueue *queue)
{
    return queue == NULL ? 0 : queue->count;
}

bool index_queue_push(IndexQueue *queue, size_t value, SchedulerError *error)
{
    size_t tail;

    if (queue == NULL) {
        error_set(error, EXIT_CODE_INTERNAL, "index queue is not initialized");
        return false;
    }
    if (!index_queue_grow(queue, error)) {
        return false;
    }
    tail = (queue->head + queue->count) % queue->capacity;
    queue->items[tail] = value;
    ++queue->count;
    return true;
}

bool index_queue_peek(const IndexQueue *queue, size_t *value)
{
    if (queue == NULL || value == NULL || queue->count == 0) {
        return false;
    }
    *value = queue->items[queue->head];
    return true;
}

bool index_queue_pop(IndexQueue *queue, size_t *value)
{
    if (queue == NULL || value == NULL || queue->count == 0) {
        return false;
    }
    *value = queue->items[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    --queue->count;
    if (queue->count == 0) {
        queue->head = 0;
    }
    return true;
}

bool index_queue_contains(const IndexQueue *queue, size_t value)
{
    size_t index;

    if (queue == NULL) {
        return false;
    }
    for (index = 0; index < queue->count; ++index) {
        if (queue->items[(queue->head + index) % queue->capacity] == value) {
            return true;
        }
    }
    return false;
}
