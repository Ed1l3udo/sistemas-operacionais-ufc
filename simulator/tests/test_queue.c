#include "test_common.h"

#include "queue.h"

static void test_empty_and_fifo(void)
{
    IndexQueue queue;
    SchedulerError error;
    size_t value;

    index_queue_init(&queue);
    error_clear(&error);
    TEST_ASSERT(index_queue_is_empty(&queue));
    TEST_ASSERT(index_queue_size(&queue) == 0);
    TEST_ASSERT(!index_queue_peek(&queue, &value));
    TEST_ASSERT(!index_queue_pop(&queue, &value));
    TEST_ASSERT(index_queue_push(&queue, 0, &error));
    TEST_ASSERT(index_queue_push(&queue, 42, &error));
    TEST_ASSERT(index_queue_contains(&queue, 0));
    TEST_ASSERT(index_queue_peek(&queue, &value) && value == 0);
    TEST_ASSERT(index_queue_pop(&queue, &value) && value == 0);
    TEST_ASSERT(index_queue_pop(&queue, &value) && value == 42);
    TEST_ASSERT(index_queue_is_empty(&queue));
    index_queue_destroy(&queue);
    index_queue_destroy(&queue);
}

static void test_wrap_around_and_growth(void)
{
    IndexQueue queue;
    SchedulerError error;
    size_t value;
    size_t index;

    index_queue_init(&queue);
    error_clear(&error);
    for (index = 0; index < 8; ++index) {
        TEST_ASSERT(index_queue_push(&queue, index, &error));
    }
    for (index = 0; index < 3; ++index) {
        TEST_ASSERT(index_queue_pop(&queue, &value) && value == index);
    }
    for (index = 8; index < 17; ++index) {
        TEST_ASSERT(index_queue_push(&queue, index, &error));
    }
    for (index = 3; index < 17; ++index) {
        TEST_ASSERT(index_queue_pop(&queue, &value) && value == index);
    }
    TEST_ASSERT(index_queue_is_empty(&queue));
    index_queue_destroy(&queue);
}

void run_queue_tests(void)
{
    test_empty_and_fifo();
    test_wrap_around_and_growth();
}
