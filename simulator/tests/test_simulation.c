#include "test_common.h"

#include "simulation.h"

static bool append_processes(ProcessList *list, const int values[][3],
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

static void test_initialization_and_admission(void)
{
    const int values[][3] = {{3, 2, 1}, {0, 1, 2}, {0, 3, 1}};
    ProcessList original;
    Simulation simulation;
    IndexList admitted;
    SchedulerError error;

    error_clear(&error);
    TEST_ASSERT(append_processes(&original, values, 3, &error));
    TEST_ASSERT(simulation_init(&simulation, &original, &error));
    TEST_ASSERT(simulation.current_time == 0);
    TEST_ASSERT(simulation.current_process_index == SIMULATION_NO_PROCESS);
    TEST_ASSERT(simulation.completed_count == 0 && simulation.timeline.count == 0);
    TEST_ASSERT(simulation.processes.items != original.items);
    simulation.processes.items[0].remaining_time = 1;
    TEST_ASSERT(original.items[0].remaining_time == 2);
    simulation.processes.items[0].remaining_time = 2;
    index_list_init(&admitted);
    TEST_ASSERT(simulation_admit_current(&simulation, &admitted, &error));
    TEST_ASSERT(admitted.count == 2);
    TEST_ASSERT(admitted.items[0] == 1 && admitted.items[1] == 2);
    TEST_ASSERT(simulation.processes.items[0].status == PROCESS_NEW);
    TEST_ASSERT(simulation.processes.items[1].status == PROCESS_READY);
    TEST_ASSERT(simulation_validate(&simulation, &error));
    TEST_ASSERT(simulation_admit_current(&simulation, &admitted, &error));
    TEST_ASSERT(admitted.count == 0);
    index_list_destroy(&admitted);
    simulation_destroy(&simulation);
    process_list_destroy(&original);
}

static void test_dispatch_execution_and_completion(void)
{
    const int values[][3] = {{0, 3, 1}};
    ProcessList original;
    Simulation simulation;
    SchedulerError error;
    const TimelineEntry *entry;

    error_clear(&error);
    TEST_ASSERT(append_processes(&original, values, 1, &error));
    TEST_ASSERT(simulation_init(&simulation, &original, &error));
    TEST_ASSERT(!simulation_dispatch(&simulation, 0, &error));
    TEST_ASSERT(!simulation_execute_second(&simulation, &error));
    TEST_ASSERT(simulation_admit_current(&simulation, NULL, &error));
    TEST_ASSERT(simulation_dispatch(&simulation, 0, &error));
    TEST_ASSERT(!simulation_dispatch(&simulation, 0, &error));
    TEST_ASSERT(simulation_execute_second(&simulation, &error));
    TEST_ASSERT(simulation.current_time == 1);
    TEST_ASSERT(simulation.processes.items[0].first_execution == 0);
    TEST_ASSERT(simulation.processes.items[0].remaining_time == 2);
    TEST_ASSERT(simulation.processes.items[0].status == PROCESS_RUNNING);
    TEST_ASSERT(simulation_execute_second(&simulation, &error));
    TEST_ASSERT(simulation.current_time == 2 &&
                simulation.processes.items[0].remaining_time == 1);
    TEST_ASSERT(simulation_execute_second(&simulation, &error));
    TEST_ASSERT(simulation.current_time == 3 && simulation.completed_count == 1);
    TEST_ASSERT(simulation.current_process_index == SIMULATION_NO_PROCESS);
    TEST_ASSERT(simulation.processes.items[0].status == PROCESS_FINISHED);
    TEST_ASSERT(simulation.processes.items[0].completion_time == 3);
    TEST_ASSERT(simulation_is_complete(&simulation));
    TEST_ASSERT(!simulation_dispatch(&simulation, 0, &error));
    entry = timeline_get(&simulation.timeline, 2);
    TEST_ASSERT(entry != NULL && entry->start_time == 2 && entry->process_id == 1);
    TEST_ASSERT(timeline_get(&simulation.timeline, 3) == NULL);
    TEST_ASSERT(simulation_validate(&simulation, &error));
    simulation_destroy(&simulation);
    process_list_destroy(&original);
}

static void test_preemption_controlled_manually(void)
{
    const int values[][3] = {{0, 2, 1}, {0, 1, 1}};
    ProcessList original;
    Simulation simulation;
    SchedulerError error;
    size_t preempted;

    error_clear(&error);
    TEST_ASSERT(append_processes(&original, values, 2, &error));
    TEST_ASSERT(simulation_init(&simulation, &original, &error));
    TEST_ASSERT(simulation_admit_current(&simulation, NULL, &error));
    TEST_ASSERT(simulation_dispatch(&simulation, 0, &error));
    TEST_ASSERT(simulation_execute_second(&simulation, &error));
    TEST_ASSERT(simulation_preempt_current(&simulation, &preempted, &error));
    TEST_ASSERT(preempted == 0 && simulation.processes.items[0].remaining_time == 1);
    TEST_ASSERT(simulation.current_time == 1 && simulation.timeline.count == 1);
    TEST_ASSERT(simulation.processes.items[0].status == PROCESS_READY);
    TEST_ASSERT(simulation_dispatch(&simulation, 1, &error));
    TEST_ASSERT(simulation_execute_second(&simulation, &error));
    TEST_ASSERT(simulation_dispatch(&simulation, 0, &error));
    TEST_ASSERT(simulation_execute_second(&simulation, &error));
    TEST_ASSERT(simulation_is_complete(&simulation));
    TEST_ASSERT(simulation.timeline.items[0].process_id == 1);
    TEST_ASSERT(simulation.timeline.items[1].process_id == 2);
    TEST_ASSERT(simulation.timeline.items[2].process_id == 1);
    TEST_ASSERT(!simulation_preempt_current(&simulation, NULL, &error));
    TEST_ASSERT(simulation_validate(&simulation, &error));
    simulation_destroy(&simulation);
    process_list_destroy(&original);
}

static void test_idle_time_and_late_arrival(void)
{
    const int values[][3] = {{2, 1, 1}};
    ProcessList original;
    Simulation simulation;
    SchedulerError error;
    int64_t next_arrival;

    error_clear(&error);
    TEST_ASSERT(append_processes(&original, values, 1, &error));
    TEST_ASSERT(simulation_init(&simulation, &original, &error));
    TEST_ASSERT(simulation_record_idle_second(&simulation, &error));
    TEST_ASSERT(simulation_record_idle_second(&simulation, &error));
    TEST_ASSERT(simulation.current_time == 2 && simulation.timeline.count == 2);
    TEST_ASSERT(simulation.timeline.items[0].process_id == TIMELINE_IDLE_PROCESS_ID);
    TEST_ASSERT(simulation_next_arrival(&simulation, &next_arrival));
    TEST_ASSERT(next_arrival == 2);
    TEST_ASSERT(simulation_admit_current(&simulation, NULL, &error));
    TEST_ASSERT(!simulation_record_idle_second(&simulation, &error));
    TEST_ASSERT(simulation_dispatch(&simulation, 0, &error));
    TEST_ASSERT(simulation_execute_second(&simulation, &error));
    TEST_ASSERT(simulation.timeline.items[2].process_id == 1);
    TEST_ASSERT(simulation_validate(&simulation, &error));
    simulation_destroy(&simulation);
    process_list_destroy(&original);
}

static void test_idle_time_between_processes(void)
{
    const int values[][3] = {{0, 1, 1}, {3, 1, 1}};
    ProcessList original;
    Simulation simulation;
    SchedulerError error;

    error_clear(&error);
    TEST_ASSERT(append_processes(&original, values, 2, &error));
    TEST_ASSERT(simulation_init(&simulation, &original, &error));
    TEST_ASSERT(simulation_admit_current(&simulation, NULL, &error));
    TEST_ASSERT(simulation_dispatch(&simulation, 0, &error));
    TEST_ASSERT(simulation_execute_second(&simulation, &error));
    TEST_ASSERT(simulation_record_idle_second(&simulation, &error));
    TEST_ASSERT(simulation_record_idle_second(&simulation, &error));
    TEST_ASSERT(simulation.current_time == 3);
    TEST_ASSERT(simulation_admit_current(&simulation, NULL, &error));
    TEST_ASSERT(simulation_dispatch(&simulation, 1, &error));
    TEST_ASSERT(simulation_execute_second(&simulation, &error));
    TEST_ASSERT(simulation.timeline.items[0].process_id == 1);
    TEST_ASSERT(simulation.timeline.items[1].process_id == TIMELINE_IDLE_PROCESS_ID);
    TEST_ASSERT(simulation.timeline.items[2].process_id == TIMELINE_IDLE_PROCESS_ID);
    TEST_ASSERT(simulation.timeline.items[3].process_id == 2);
    TEST_ASSERT(simulation_is_complete(&simulation));
    simulation_destroy(&simulation);
    process_list_destroy(&original);
}

static void test_timeline_rules(void)
{
    Timeline timeline;
    SchedulerError error;
    size_t index;

    timeline_init(&timeline);
    error_clear(&error);
    for (index = 0; index < 20; ++index) {
        TEST_ASSERT(timeline_append(&timeline, (int64_t)index,
                                    TIMELINE_IDLE_PROCESS_ID, &error));
    }
    TEST_ASSERT(timeline.count == 20 && timeline.capacity >= 20);
    TEST_ASSERT(!timeline_append(&timeline, 22, 1, &error));
    TEST_ASSERT(!timeline_append(&timeline, 20, -1, &error));
    timeline_destroy(&timeline);
}

void run_simulation_tests(void)
{
    test_initialization_and_admission();
    test_dispatch_execution_and_completion();
    test_preemption_controlled_manually();
    test_idle_time_and_late_arrival();
    test_idle_time_between_processes();
    test_timeline_rules();
}
