#include "test_common.h"

#include <string.h>

#include "process.h"

static bool parse_processes(const char *text, ProcessList *list,
                            SchedulerError *error)
{
    FILE *stream = test_stream_from_text(text);
    bool parsed;

    TEST_ASSERT(stream != NULL);
    if (stream == NULL) {
        return false;
    }
    parsed = process_list_read(stream, list, error);
    TEST_ASSERT(fclose(stream) == 0);
    return parsed;
}

static void test_valid_processes_and_initialization(void)
{
    ProcessList list;
    SchedulerError error;
    const Process *first;
    const Process *second;

    process_list_init(&list);
    error_clear(&error);
    TEST_ASSERT(parse_processes("0 5 2\n3 4 1\n", &list, &error));
    TEST_ASSERT(list.count == 2);
    first = process_list_get_const(&list, 0);
    second = process_list_get_const(&list, 1);
    TEST_ASSERT(first != NULL && second != NULL);
    TEST_ASSERT(first->id == 1 && second->id == 2);
    TEST_ASSERT(first->arrival_time == 0 && second->arrival_time == 3);
    TEST_ASSERT(first->burst_time == 5 && first->remaining_time == 5);
    TEST_ASSERT(first->static_priority == 2 && first->dynamic_priority == 2);
    TEST_ASSERT(first->first_execution == -1 && first->completion_time == -1);
    TEST_ASSERT(first->status == PROCESS_NEW);
    process_list_destroy(&list);
}

static void test_order_whitespace_and_blank_lines(void)
{
    ProcessList list;
    SchedulerError error;

    process_list_init(&list);
    error_clear(&error);
    TEST_ASSERT(parse_processes("\n  4\t2  3\n\t\n0 1 1\n", &list, &error));
    TEST_ASSERT(list.count == 2);
    TEST_ASSERT(list.items[0].id == 1 && list.items[0].arrival_time == 4);
    TEST_ASSERT(list.items[1].id == 2 && list.items[1].arrival_time == 0);
    process_list_destroy(&list);
}

static void test_invalid_process_inputs(void)
{
    static const char *cases[] = {
        "0 1 0\n", "-1 1 1\n", "0 0 1\n", "0 -1 1\n",
        "0 1 -1\n", "0 1\n", "0 1 1 extra\n", "a 1 1\n",
        "999999999999999999999 1 1\n", "\n\t \n"
    };
    size_t index;

    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        ProcessList list;
        SchedulerError error;

        process_list_init(&list);
        error_clear(&error);
        TEST_ASSERT(!parse_processes(cases[index], &list, &error));
        TEST_ASSERT(error.code == EXIT_CODE_INPUT);
        TEST_ASSERT(error.message[0] != '\0');
        process_list_destroy(&list);
    }
}

static void test_growth_and_deep_copy(void)
{
    ProcessList original;
    ProcessList copy;
    SchedulerError error;
    int index;

    process_list_init(&original);
    process_list_init(&copy);
    error_clear(&error);
    for (index = 0; index < 24; ++index) {
        TEST_ASSERT(process_list_append(&original, index, 1, 1, &error));
    }
    TEST_ASSERT(original.count == 24);
    TEST_ASSERT(original.capacity >= original.count);
    TEST_ASSERT(process_list_clone(&original, &copy, &error));
    TEST_ASSERT(copy.count == original.count);
    copy.items[0].remaining_time = 99;
    copy.items[0].dynamic_priority = 7;
    TEST_ASSERT(original.items[0].remaining_time == 1);
    TEST_ASSERT(original.items[0].dynamic_priority == 1);
    TEST_ASSERT(process_list_get(&original, original.count) == NULL);
    TEST_ASSERT(process_list_get_const(NULL, 0) == NULL);
    process_list_destroy(&copy);
    process_list_destroy(&original);
}

void run_process_tests(void)
{
    test_valid_processes_and_initialization();
    test_order_whitespace_and_blank_lines();
    test_invalid_process_inputs();
    test_growth_and_deep_copy();
}
