#include "test_common.h"

#include <string.h>

#include "scheduler.h"

static bool build_list(ProcessList *list, const int values[][3], size_t count,
                       SchedulerError *error)
{
    size_t index;

    process_list_init(list);
    for (index = 0; index < count; ++index) {
        if (!process_list_append(list, values[index][0], values[index][1],
                                 values[index][2], error)) {
            return false;
        }
    }
    return true;
}

static void assert_timeline(const Simulation *simulation, const int *expected,
                            size_t count)
{
    size_t index;

    TEST_ASSERT(simulation->timeline.count == count);
    for (index = 0; index < count && index < simulation->timeline.count; ++index) {
        TEST_ASSERT(simulation->timeline.items[index].process_id == expected[index]);
    }
}

static void assert_finished(const Simulation *simulation, SchedulerError *error)
{
    TEST_ASSERT(simulation_is_complete(simulation));
    TEST_ASSERT(simulation->current_process_index == SIMULATION_NO_PROCESS);
    TEST_ASSERT(simulation_validate(simulation, error));
}

static void test_plain_reference_and_boundary(void)
{
    const int values[][3] = {{0, 5, 2}, {0, 2, 3}, {1, 4, 1}, {3, 3, 4}};
    const int reference[] = {1, 1, 2, 2, 3, 3, 1, 1, 4, 4, 3, 3, 1, 4};
    const int boundary_values[][3] = {{0, 4, 1}, {2, 1, 1}};
    const int boundary[] = {1, 1, 2, 1, 1};
    ProcessList source;
    Process original[4];
    Simulation result;
    SchedulerError error;

    error_clear(&error);
    TEST_ASSERT(build_list(&source, values, 4, &error));
    memcpy(original, source.items, sizeof(original));
    TEST_ASSERT(scheduler_run_round_robin(&source, 2, &result, &error));
    assert_timeline(&result, reference, 14);
    assert_finished(&result, &error);
    TEST_ASSERT(memcmp(source.items, original, sizeof(original)) == 0);
    simulation_destroy(&result);
    process_list_destroy(&source);
    TEST_ASSERT(build_list(&source, boundary_values, 2, &error));
    TEST_ASSERT(scheduler_run_round_robin(&source, 2, &result, &error));
    assert_timeline(&result, boundary, 5);
    simulation_destroy(&result);
    process_list_destroy(&source);
}

static void test_plain_quantum_and_idle(void)
{
    const int values[][3] = {{2, 1, 1}, {0, 1, 1}};
    const int expected[] = {2, 0, 1};
    ProcessList source;
    Simulation result;
    SchedulerError error;

    error_clear(&error);
    TEST_ASSERT(build_list(&source, values, 2, &error));
    TEST_ASSERT(scheduler_run_round_robin(&source, 1, &result, &error));
    assert_timeline(&result, expected, 3);
    assert_finished(&result, &error);
    simulation_destroy(&result);
    TEST_ASSERT(!scheduler_run_round_robin(&source, 0, &result, &error));
    process_list_destroy(&source);
}

static void test_priority_reference_and_aging(void)
{
    const int values[][3] = {{0, 5, 2}, {0, 2, 3}, {1, 4, 1}, {3, 3, 4}};
    const int reference[] = {1, 1, 3, 3, 2, 2, 1, 1, 4, 4, 3, 3, 1, 4};
    ProcessList source;
    Process original[4];
    Simulation result;
    SchedulerError error;
    size_t index;

    error_clear(&error);
    TEST_ASSERT(build_list(&source, values, 4, &error));
    memcpy(original, source.items, sizeof(original));
    TEST_ASSERT(scheduler_run_priority_round_robin(&source, 2, 1, &result,
                                                    &error));
    assert_timeline(&result, reference, 14);
    assert_finished(&result, &error);
    for (index = 0; index < result.processes.count; ++index) {
        TEST_ASSERT(result.processes.items[index].static_priority == values[index][2]);
        TEST_ASSERT(result.processes.items[index].dynamic_priority >= 1);
    }
    TEST_ASSERT(memcmp(source.items, original, sizeof(original)) == 0);
    simulation_destroy(&result);
    process_list_destroy(&source);
}

static void test_priority_aging_boundaries(void)
{
    const int exact_values[][3] = {{0, 2, 1}, {0, 1, 3}};
    const int early_values[][3] = {{0, 1, 1}, {0, 1, 3}};
    const int fifo_values[][3] = {{0, 1, 1}, {0, 1, 1}};
    const int fifo_expected[] = {1, 2};
    ProcessList source;
    Simulation result;
    SchedulerError error;

    error_clear(&error);
    TEST_ASSERT(build_list(&source, exact_values, 2, &error));
    TEST_ASSERT(scheduler_run_priority_round_robin(&source, 2, 9, &result,
                                                    &error));
    TEST_ASSERT(result.processes.items[1].dynamic_priority == 1);
    simulation_destroy(&result);
    process_list_destroy(&source);
    TEST_ASSERT(build_list(&source, early_values, 2, &error));
    TEST_ASSERT(scheduler_run_priority_round_robin(&source, 2, 1, &result,
                                                    &error));
    TEST_ASSERT(result.processes.items[1].dynamic_priority == 3);
    simulation_destroy(&result);
    process_list_destroy(&source);
    TEST_ASSERT(build_list(&source, fifo_values, 2, &error));
    TEST_ASSERT(scheduler_run_priority_round_robin(&source, 1, 0, &result,
                                                    &error));
    assert_timeline(&result, fifo_expected, 2);
    simulation_destroy(&result);
    TEST_ASSERT(!scheduler_run_priority_round_robin(&source, 0, 0, &result,
                                                     &error));
    TEST_ASSERT(!scheduler_run_priority_round_robin(&source, 1, -1, &result,
                                                     &error));
    process_list_destroy(&source);
}

void run_round_robin_tests(void)
{
    test_plain_reference_and_boundary();
    test_plain_quantum_and_idle();
    test_priority_reference_and_aging();
    test_priority_aging_boundaries();
}
