#include "test_common.h"

#include <stdlib.h>
#include <string.h>

#include "cli.h"
#include "metrics.h"
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

static bool close_enough(double actual, double expected)
{
    double difference = actual - expected;

    return difference >= -0.0001 && difference <= 0.0001;
}

static bool run_algorithm(AlgorithmType algorithm, const ProcessList *source,
                          Simulation *simulation, SchedulerError *error)
{
    switch (algorithm) {
    case ALGORITHM_FCFS:
        return scheduler_run_fcfs(source, 9, simulation, error);
    case ALGORITHM_SJF:
        return scheduler_run_sjf(source, 9, simulation, error);
    case ALGORITHM_SRTF:
        return scheduler_run_srtf(source, 9, simulation, error);
    case ALGORITHM_PRIORITY_NON_PREEMPTIVE:
        return scheduler_run_priority_non_preemptive(source, 9, simulation, error);
    case ALGORITHM_PRIORITY_PREEMPTIVE:
        return scheduler_run_priority_preemptive(source, 9, simulation, error);
    case ALGORITHM_RR:
        return scheduler_run_round_robin(source, 2, simulation, error);
    case ALGORITHM_PRIORITY_RR:
        return scheduler_run_priority_round_robin(source, 2, 1, simulation, error);
    case ALGORITHM_ALL:
        break;
    }
    return false;
}

static void test_individual_metrics_and_reference_averages(void)
{
    const int values[][3] = {{0, 5, 2}, {0, 2, 3}, {1, 4, 1}, {3, 3, 4}};
    const AlgorithmType algorithms[] = {
        ALGORITHM_FCFS, ALGORITHM_SJF, ALGORITHM_SRTF,
        ALGORITHM_PRIORITY_NON_PREEMPTIVE, ALGORITHM_PRIORITY_PREEMPTIVE,
        ALGORITHM_RR, ALGORITHM_PRIORITY_RR
    };
    const double expected_turnaround[] = {7.50, 6.75, 6.75, 8.75, 8.75, 9.75,
                                          10.25};
    const double expected_waiting[] = {4.00, 3.25, 3.25, 5.25, 5.25, 6.25,
                                       6.75};
    const double expected_response[] = {4.00, 3.25, 3.25, 5.25, 4.25, 2.50,
                                        2.50};
    const size_t expected_switches[] = {3, 3, 3, 3, 4, 7, 7};
    ProcessList source;
    Process original[4];
    SchedulerError error;
    size_t index;

    error_clear(&error);
    TEST_ASSERT(build_list(&source, values, 4, &error));
    memcpy(original, source.items, sizeof(original));
    for (index = 0; index < sizeof(algorithms) / sizeof(algorithms[0]); ++index) {
        Simulation simulation = {0};
        MetricsReport report;

        metrics_report_init(&report);
        TEST_ASSERT(run_algorithm(algorithms[index], &source, &simulation, &error));
        TEST_ASSERT(metrics_calculate(&simulation, &report, &error));
        TEST_ASSERT(close_enough(report.average_turnaround,
                                 expected_turnaround[index]));
        TEST_ASSERT(close_enough(report.average_waiting, expected_waiting[index]));
        TEST_ASSERT(close_enough(report.average_response,
                                 expected_response[index]));
        TEST_ASSERT(report.context_switches == expected_switches[index]);
        if (algorithms[index] == ALGORITHM_FCFS) {
            TEST_ASSERT(report.processes[0].process_id == 1);
            TEST_ASSERT(report.processes[0].turnaround_time == 7);
            TEST_ASSERT(report.processes[0].waiting_time == 2);
            TEST_ASSERT(report.processes[0].response_time == 2);
            TEST_ASSERT(report.processes[1].turnaround_time == 2);
            TEST_ASSERT(report.processes[2].waiting_time == 6);
        }
        TEST_ASSERT(memcmp(source.items, original, sizeof(original)) == 0);
        metrics_report_destroy(&report);
        simulation_destroy(&simulation);
    }
    process_list_destroy(&source);
}

static bool build_finished_timeline(Simulation *simulation, const int *timeline,
                                    size_t timeline_count, size_t process_count,
                                    SchedulerError *error)
{
    size_t *counts;
    size_t *first;
    size_t *last;
    size_t index;

    process_list_init(&simulation->processes);
    timeline_init(&simulation->timeline);
    simulation->current_time = (int64_t)timeline_count;
    simulation->completed_count = process_count;
    simulation->current_process_index = SIMULATION_NO_PROCESS;
    counts = calloc(process_count, sizeof(*counts));
    first = calloc(process_count, sizeof(*first));
    last = calloc(process_count, sizeof(*last));
    if (counts == NULL || first == NULL || last == NULL) {
        free(last);
        free(first);
        free(counts);
        return false;
    }
    for (index = 0; index < process_count; ++index) {
        first[index] = SIZE_MAX;
    }
    for (index = 0; index < timeline_count; ++index) {
        int id = timeline[index];

        if (id > 0) {
            if ((size_t)id > process_count) {
                free(last);
                free(first);
                free(counts);
                return false;
            }
            ++counts[(size_t)id - 1];
            if (first[(size_t)id - 1] == SIZE_MAX) {
                first[(size_t)id - 1] = index;
            }
            last[(size_t)id - 1] = index;
        }
    }
    for (index = 0; index < process_count; ++index) {
        if (counts[index] == 0 || !process_list_append(&simulation->processes, 0,
                                                        (int)counts[index], 1,
                                                        error)) {
            free(last);
            free(first);
            free(counts);
            return false;
        }
        simulation->processes.items[index].status = PROCESS_FINISHED;
        simulation->processes.items[index].remaining_time = 0;
        simulation->processes.items[index].first_execution = (int64_t)first[index];
        simulation->processes.items[index].completion_time = (int64_t)last[index] + 1;
    }
    for (index = 0; index < timeline_count; ++index) {
        if (!timeline_append(&simulation->timeline, (int64_t)index, timeline[index],
                             error)) {
            free(last);
            free(first);
            free(counts);
            return false;
        }
    }
    free(last);
    free(first);
    free(counts);
    return true;
}

static void test_context_switch_definition(void)
{
    static const int traces[][5] = {
        {1, 0, 0, 0, 0}, {1, 1, 1, 0, 0}, {1, 2, 0, 0, 0},
        {1, 2, 1, 0, 0}, {0, 1, 0, 0, 0}, {1, 0, 0, 0, 0},
        {1, 0, 2, 0, 0}, {0, 1, 2, 0, 3}, {1, 1, 2, 2, 3}
    };
    static const size_t lengths[] = {1, 3, 2, 3, 2, 2, 3, 5, 5};
    static const size_t process_counts[] = {1, 1, 2, 2, 1, 1, 2, 3, 3};
    static const size_t expected[] = {0, 0, 1, 2, 0, 0, 0, 1, 2};
    SchedulerError error;
    size_t index;

    error_clear(&error);
    for (index = 0; index < sizeof(expected) / sizeof(expected[0]); ++index) {
        Simulation simulation = {0};
        MetricsReport report;

        metrics_report_init(&report);
        TEST_ASSERT(build_finished_timeline(&simulation, traces[index], lengths[index],
                                            process_counts[index], &error));
        TEST_ASSERT(metrics_calculate(&simulation, &report, &error));
        TEST_ASSERT(report.context_switches == expected[index]);
        metrics_report_destroy(&report);
        simulation_destroy(&simulation);
    }
}

static void test_rejections_and_safe_destruction(void)
{
    const int values[][3] = {{0, 2, 1}};
    ProcessList source;
    Simulation simulation = {0};
    Simulation empty = {0};
    MetricsReport report;
    SchedulerError error;

    error_clear(&error);
    metrics_report_init(&report);
    metrics_report_destroy(&report);
    metrics_report_destroy(&report);
    process_list_init(&empty.processes);
    timeline_init(&empty.timeline);
    TEST_ASSERT(!metrics_calculate(&empty, &report, &error));
    TEST_ASSERT(build_list(&source, values, 1, &error));
    TEST_ASSERT(scheduler_run_fcfs(&source, 1, &simulation, &error));
    simulation.processes.items[0].completion_time =
        simulation.processes.items[0].first_execution;
    TEST_ASSERT(!metrics_calculate(&simulation, &report, &error));
    simulation.processes.items[0].completion_time = 2;
    simulation.timeline.items[0].start_time = 4;
    TEST_ASSERT(!metrics_calculate(&simulation, &report, &error));
    metrics_report_destroy(&report);
    simulation_destroy(&empty);
    simulation_destroy(&simulation);
    process_list_destroy(&source);
}

void run_metrics_tests(void)
{
    test_individual_metrics_and_reference_averages();
    test_context_switch_definition();
    test_rejections_and_safe_destruction();
}
