#include "test_common.h"

#include <string.h>

#include "scheduler.h"

typedef bool (*PreemptiveScheduler)(const ProcessList *, uint64_t, Simulation *,
                                    SchedulerError *);

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
        TEST_ASSERT(simulation->timeline.items[index].start_time == (int64_t)index);
        TEST_ASSERT(simulation->timeline.items[index].process_id == expected[index]);
    }
}

static void assert_result(const Simulation *result, SchedulerError *error)
{
    TEST_ASSERT(simulation_is_complete(result));
    TEST_ASSERT(result->current_process_index == SIMULATION_NO_PROCESS);
    TEST_ASSERT(simulation_validate(result, error));
}

static void test_reference_timelines(void)
{
    const int values[][3] = {{0, 5, 2}, {0, 2, 3}, {1, 4, 1}, {3, 3, 4}};
    const int srtf[] = {2, 2, 3, 3, 3, 3, 4, 4, 4, 1, 1, 1, 1, 1};
    const int priority[] = {1, 3, 3, 3, 3, 1, 1, 1, 1, 2, 2, 4, 4, 4};
    PreemptiveScheduler functions[] = {
        scheduler_run_srtf, scheduler_run_priority_preemptive
    };
    const int *timelines[] = {srtf, priority};
    ProcessList source;
    Process original[4];
    SchedulerError error;
    size_t index;

    error_clear(&error);
    TEST_ASSERT(build_list(&source, values, 4, &error));
    memcpy(original, source.items, sizeof(original));
    for (index = 0; index < 2; ++index) {
        Simulation result;

        TEST_ASSERT(functions[index](&source, 7, &result, &error));
        assert_timeline(&result, timelines[index], 14);
        assert_result(&result, &error);
        TEST_ASSERT(memcmp(source.items, original, sizeof(original)) == 0);
        simulation_destroy(&result);
    }
    process_list_destroy(&source);
}

static void test_srtf_preemption_and_current_tie(void)
{
    const int preemption_values[][3] = {{0, 8, 1}, {2, 2, 1}};
    const int preemption_timeline[] = {1, 1, 2, 2, 1, 1, 1, 1, 1, 1};
    const int tie_values[][3] = {{0, 4, 1}, {1, 3, 1}};
    const int tie_timeline[] = {1, 1, 1, 1, 2, 2, 2};
    ProcessList source;
    Simulation result;
    SchedulerError error;

    error_clear(&error);
    TEST_ASSERT(build_list(&source, preemption_values, 2, &error));
    TEST_ASSERT(scheduler_run_srtf(&source, 1, &result, &error));
    assert_timeline(&result, preemption_timeline, 10);
    TEST_ASSERT(result.processes.items[0].first_execution == 0);
    assert_result(&result, &error);
    simulation_destroy(&result);
    process_list_destroy(&source);
    TEST_ASSERT(build_list(&source, tie_values, 2, &error));
    TEST_ASSERT(scheduler_run_srtf(&source, 1, &result, &error));
    assert_timeline(&result, tie_timeline, 7);
    assert_result(&result, &error);
    simulation_destroy(&result);
    process_list_destroy(&source);
}

static void test_priority_preemption_and_idle(void)
{
    const int values[][3] = {{0, 6, 3}, {2, 2, 1}, {3, 1, 2}};
    const int expected[] = {1, 1, 2, 2, 3, 1, 1, 1, 1};
    const int same_priority_values[][3] = {{0, 4, 2}, {1, 1, 2}};
    const int same_priority_expected[] = {1, 1, 1, 1, 2};
    const int idle_values[][3] = {{3, 1, 1}, {0, 1, 2}};
    const int idle_expected[] = {2, 0, 0, 1};
    ProcessList source;
    Simulation result;
    SchedulerError error;

    error_clear(&error);
    TEST_ASSERT(build_list(&source, values, 3, &error));
    TEST_ASSERT(scheduler_run_priority_preemptive(&source, 4, &result, &error));
    assert_timeline(&result, expected, 9);
    assert_result(&result, &error);
    simulation_destroy(&result);
    process_list_destroy(&source);
    TEST_ASSERT(build_list(&source, same_priority_values, 2, &error));
    TEST_ASSERT(scheduler_run_priority_preemptive(&source, 4, &result, &error));
    assert_timeline(&result, same_priority_expected, 5);
    simulation_destroy(&result);
    process_list_destroy(&source);
    TEST_ASSERT(build_list(&source, idle_values, 2, &error));
    TEST_ASSERT(scheduler_run_srtf(&source, 4, &result, &error));
    assert_timeline(&result, idle_expected, 4);
    assert_result(&result, &error);
    simulation_destroy(&result);
    process_list_destroy(&source);
}

static void test_seed_reproducibility(void)
{
    const int values[][3] = {{0, 2, 1}, {0, 2, 1}, {0, 2, 1}};
    ProcessList source;
    Simulation first;
    Simulation second;
    SchedulerError error;

    error_clear(&error);
    TEST_ASSERT(build_list(&source, values, 3, &error));
    TEST_ASSERT(scheduler_run_srtf(&source, 88, &first, &error));
    TEST_ASSERT(scheduler_run_srtf(&source, 88, &second, &error));
    TEST_ASSERT(first.timeline.count == second.timeline.count);
    TEST_ASSERT(memcmp(first.timeline.items, second.timeline.items,
                       first.timeline.count * sizeof(*first.timeline.items)) == 0);
    simulation_destroy(&second);
    simulation_destroy(&first);
    process_list_destroy(&source);
}

void run_preemptive_tests(void)
{
    test_reference_timelines();
    test_srtf_preemption_and_current_tie();
    test_priority_preemption_and_idle();
    test_seed_reproducibility();
}
