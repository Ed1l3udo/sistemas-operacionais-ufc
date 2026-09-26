#include "test_common.h"

#include <string.h>

#include "scheduler.h"

typedef bool (*SchedulerFunction)(const ProcessList *, uint64_t, Simulation *,
                                  SchedulerError *);

static bool build_processes(ProcessList *list, const int values[][3],
                            size_t count, SchedulerError *error)
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

static void assert_finished(const Simulation *simulation, SchedulerError *error)
{
    size_t index;

    TEST_ASSERT(simulation_is_complete(simulation));
    TEST_ASSERT(simulation->current_process_index == SIMULATION_NO_PROCESS);
    TEST_ASSERT(simulation_validate(simulation, error));
    for (index = 0; index < simulation->processes.count; ++index) {
        const Process *process = &simulation->processes.items[index];

        TEST_ASSERT(process->status == PROCESS_FINISHED);
        TEST_ASSERT(process->remaining_time == 0);
        TEST_ASSERT(process->first_execution >= process->arrival_time);
        TEST_ASSERT(process->completion_time > process->first_execution);
        TEST_ASSERT(process->dynamic_priority == process->static_priority);
    }
}

static void test_reference_timelines(void)
{
    const int values[][3] = {{0, 5, 2}, {0, 2, 3}, {1, 4, 1}, {3, 3, 4}};
    const int fcfs[] = {2, 2, 1, 1, 1, 1, 1, 3, 3, 3, 3, 4, 4, 4};
    const int sjf[] = {2, 2, 3, 3, 3, 3, 4, 4, 4, 1, 1, 1, 1, 1};
    const int priority[] = {1, 1, 1, 1, 1, 3, 3, 3, 3, 2, 2, 4, 4, 4};
    SchedulerFunction functions[] = {
        scheduler_run_fcfs, scheduler_run_sjf,
        scheduler_run_priority_non_preemptive
    };
    const int *timelines[] = {fcfs, sjf, priority};
    ProcessList source;
    Process original[4];
    SchedulerError error;
    size_t index;

    error_clear(&error);
    TEST_ASSERT(build_processes(&source, values, 4, &error));
    memcpy(original, source.items, sizeof(original));
    for (index = 0; index < sizeof(functions) / sizeof(functions[0]); ++index) {
        Simulation result;

        TEST_ASSERT(functions[index](&source, 123, &result, &error));
        assert_timeline(&result, timelines[index], 14);
        assert_finished(&result, &error);
        TEST_ASSERT(memcmp(source.items, original, sizeof(original)) == 0);
        simulation_destroy(&result);
    }
    process_list_destroy(&source);
}

static void test_idle_and_non_preemption(void)
{
    const int values[][3] = {{2, 3, 3}, {3, 1, 1}};
    const int expected_fcfs[] = {0, 0, 1, 1, 1, 2};
    const int expected_sjf[] = {0, 0, 1, 1, 1, 2};
    const int expected_priority[] = {0, 0, 1, 1, 1, 2};
    SchedulerFunction functions[] = {
        scheduler_run_fcfs, scheduler_run_sjf,
        scheduler_run_priority_non_preemptive
    };
    const int *timelines[] = {expected_fcfs, expected_sjf, expected_priority};
    ProcessList source;
    SchedulerError error;
    size_t index;

    error_clear(&error);
    TEST_ASSERT(build_processes(&source, values, 2, &error));
    for (index = 0; index < sizeof(functions) / sizeof(functions[0]); ++index) {
        Simulation result;

        TEST_ASSERT(functions[index](&source, 1, &result, &error));
        assert_timeline(&result, timelines[index], 6);
        assert_finished(&result, &error);
        simulation_destroy(&result);
    }
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
    TEST_ASSERT(build_processes(&source, values, 3, &error));
    TEST_ASSERT(scheduler_run_fcfs(&source, 77, &first, &error));
    TEST_ASSERT(scheduler_run_fcfs(&source, 77, &second, &error));
    TEST_ASSERT(first.timeline.count == second.timeline.count);
    TEST_ASSERT(memcmp(first.timeline.items, second.timeline.items,
                       first.timeline.count * sizeof(*first.timeline.items)) == 0);
    simulation_destroy(&second);
    simulation_destroy(&first);
    process_list_destroy(&source);
}

void run_scheduler_tests(void)
{
    test_reference_timelines();
    test_idle_and_non_preemption();
    test_seed_reproducibility();
}
