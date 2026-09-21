#ifndef SCHEDULER_QUEUE_H
#define SCHEDULER_QUEUE_H

#include <stdbool.h>
#include <stddef.h>

#include "error.h"

typedef struct {
    size_t *items;
    size_t capacity;
    size_t count;
    size_t head;
} IndexQueue;

void index_queue_init(IndexQueue *queue);
void index_queue_destroy(IndexQueue *queue);
bool index_queue_is_empty(const IndexQueue *queue);
size_t index_queue_size(const IndexQueue *queue);
bool index_queue_push(IndexQueue *queue, size_t value, SchedulerError *error);
bool index_queue_peek(const IndexQueue *queue, size_t *value);
bool index_queue_pop(IndexQueue *queue, size_t *value);
bool index_queue_contains(const IndexQueue *queue, size_t value);

#endif
