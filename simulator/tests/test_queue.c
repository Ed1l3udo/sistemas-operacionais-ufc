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

static void test_get_and_remove_at(void)
{
    IndexQueue queue;
    SchedulerError error;
    size_t value;
    size_t expected[] = {10, 30, 40};
    size_t index;

    index_queue_init(&queue);
    error_clear(&error);
    TEST_ASSERT(index_queue_push(&queue, 10, &error));
    TEST_ASSERT(index_queue_push(&queue, 20, &error));
    TEST_ASSERT(index_queue_push(&queue, 30, &error));
    TEST_ASSERT(index_queue_push(&queue, 40, &error));
    TEST_ASSERT(index_queue_get(&queue, 2, &value) && value == 30);
    TEST_ASSERT(index_queue_remove_at(&queue, 1, &value) && value == 20);
    TEST_ASSERT(index_queue_remove_at(&queue, 9, &value) == false);
    for (index = 0; index < 3; ++index) {
        TEST_ASSERT(index_queue_pop(&queue, &value) && value == expected[index]);
    }
    TEST_ASSERT(!index_queue_remove_at(&queue, 0, &value));
    index_queue_destroy(&queue);
}

static void test_remove_at_after_wrap_around(void)
{
    IndexQueue queue;
    SchedulerError error;
    size_t value;
    size_t index;
    size_t expected[] = {3, 4, 6, 7, 8, 9};

    index_queue_init(&queue);
    error_clear(&error);
    for (index = 0; index < 8; ++index) {
        TEST_ASSERT(index_queue_push(&queue, index, &error));
    }
    for (index = 0; index < 3; ++index) {
        TEST_ASSERT(index_queue_pop(&queue, &value) && value == index);
    }
    TEST_ASSERT(index_queue_push(&queue, 8, &error));
    TEST_ASSERT(index_queue_push(&queue, 9, &error));
    TEST_ASSERT(index_queue_remove_at(&queue, 2, &value) && value == 5);
    for (index = 0; index < sizeof(expected) / sizeof(expected[0]); ++index) {
        TEST_ASSERT(index_queue_pop(&queue, &value) && value == expected[index]);
    }
    index_queue_destroy(&queue);
}

void run_queue_tests(void)
{
    test_empty_and_fifo();
    test_wrap_around_and_growth();
    test_get_and_remove_at();
    test_remove_at_after_wrap_around();
}
