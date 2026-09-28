#include "metrics.h"

#include <limits.h>
#include <stdlib.h>

static bool validate_timeline(const Simulation *simulation,
                              SchedulerError *error)
{
    size_t *executions = NULL;
    size_t index;
    bool valid = false;

    if (simulation->current_time < 0 ||
        (uintmax_t)simulation->current_time > (uintmax_t)SIZE_MAX ||
        simulation->timeline.count != (size_t)simulation->current_time) {
        error_set(error, EXIT_CODE_INTERNAL, "inconsistent simulation timeline");
        return false;
    }
    if (simulation->processes.count > SIZE_MAX / sizeof(*executions)) {
        error_set(error, EXIT_CODE_INTERNAL, "too many processes for metrics");
        return false;
    }
    executions = calloc(simulation->processes.count, sizeof(*executions));
    if (executions == NULL) {
        error_set(error, EXIT_CODE_INTERNAL,
                  "unable to allocate metrics validation data");
        return false;
    }
    for (index = 0; index < simulation->timeline.count; ++index) {
        const TimelineEntry *entry = &simulation->timeline.items[index];

        if (entry->start_time != (int64_t)index ||
            entry->process_id < TIMELINE_IDLE_PROCESS_ID ||
            entry->process_id > (int)simulation->processes.count) {
            error_set(error, EXIT_CODE_INTERNAL, "inconsistent simulation timeline");
            goto cleanup;
        }
        if (entry->process_id != TIMELINE_IDLE_PROCESS_ID) {
            const Process *process =
                &simulation->processes.items[(size_t)entry->process_id - 1];

            if (entry->start_time < process->arrival_time ||
                entry->start_time >= process->completion_time ||
                executions[(size_t)entry->process_id - 1] == SIZE_MAX) {
                error_set(error, EXIT_CODE_INTERNAL,
                          "inconsistent simulation timeline");
                goto cleanup;
            }
            ++executions[(size_t)entry->process_id - 1];
        }
    }
    for (index = 0; index < simulation->processes.count; ++index) {
        if (executions[index] != (size_t)simulation->processes.items[index].burst_time) {
            error_set(error, EXIT_CODE_INTERNAL, "inconsistent simulation timeline");
            goto cleanup;
        }
    }
    valid = true;

cleanup:
    free(executions);
    return valid;
}

static bool validate_finished_simulation(const Simulation *simulation,
                                         SchedulerError *error)
{
    size_t index;

    if (simulation == NULL || simulation->processes.items == NULL ||
        simulation->processes.count == 0 || !simulation_is_complete(simulation) ||
        simulation->current_process_index != SIMULATION_NO_PROCESS) {
        error_set(error, EXIT_CODE_INTERNAL,
                  "metrics require a completed non-empty simulation");
        return false;
    }
    for (index = 0; index < simulation->processes.count; ++index) {
        const Process *process = &simulation->processes.items[index];

        if (process->id != (int)index + 1 || process->status != PROCESS_FINISHED ||
            process->remaining_time != 0 || process->burst_time <= 0 ||
            process->first_execution < process->arrival_time ||
            process->completion_time <= process->first_execution) {
            error_set(error, EXIT_CODE_INTERNAL,
                      "invalid completed-process timestamps for metrics");
            return false;
        }
    }
    return validate_timeline(simulation, error);
}

static size_t count_context_switches(const Timeline *timeline)
{
    size_t index;
    size_t switches = 0;

    for (index = 1; index < timeline->count; ++index) {
        int previous = timeline->items[index - 1].process_id;
        int current = timeline->items[index].process_id;

        if (previous != TIMELINE_IDLE_PROCESS_ID &&
            current != TIMELINE_IDLE_PROCESS_ID && previous != current) {
            ++switches;
        }
    }
    return switches;
}

void metrics_report_init(MetricsReport *report)
{
    if (report == NULL) {
        return;
    }
    report->processes = NULL;
    report->process_count = 0;
    report->average_turnaround = 0.0;
    report->average_waiting = 0.0;
    report->average_response = 0.0;
    report->context_switches = 0;
}

void metrics_report_destroy(MetricsReport *report)
{
    if (report == NULL) {
        return;
    }
    free(report->processes);
    metrics_report_init(report);
}

bool metrics_calculate(const Simulation *simulation, MetricsReport *report,
                       SchedulerError *error)
{
    MetricsReport calculated;
    long double turnaround_sum = 0.0L;
    long double waiting_sum = 0.0L;
    long double response_sum = 0.0L;
    size_t index;

    if (report == NULL) {
        error_set(error, EXIT_CODE_INTERNAL, "invalid metrics report destination");
        return false;
    }
    if (!validate_finished_simulation(simulation, error)) {
        return false;
    }
    metrics_report_init(&calculated);
    if (simulation->processes.count > SIZE_MAX / sizeof(*calculated.processes)) {
        error_set(error, EXIT_CODE_INTERNAL, "too many process metrics");
        return false;
    }
    calculated.processes = calloc(simulation->processes.count,
                                  sizeof(*calculated.processes));
    if (calculated.processes == NULL) {
        error_set(error, EXIT_CODE_INTERNAL, "unable to allocate process metrics");
        return false;
    }
    calculated.process_count = simulation->processes.count;
    for (index = 0; index < calculated.process_count; ++index) {
        const Process *process = &simulation->processes.items[index];
        ProcessMetrics *metrics = &calculated.processes[index];

        metrics->process_id = process->id;
        metrics->turnaround_time = process->completion_time - process->arrival_time;
        metrics->waiting_time = metrics->turnaround_time - process->burst_time;
        metrics->response_time = process->first_execution - process->arrival_time;
        if (metrics->turnaround_time < 0 || metrics->waiting_time < 0 ||
            metrics->response_time < 0) {
            error_set(error, EXIT_CODE_INTERNAL, "negative process metric");
            metrics_report_destroy(&calculated);
            return false;
        }
        turnaround_sum += (long double)metrics->turnaround_time;
        waiting_sum += (long double)metrics->waiting_time;
        response_sum += (long double)metrics->response_time;
    }
    calculated.average_turnaround =
        (double)(turnaround_sum / (long double)calculated.process_count);
    calculated.average_waiting =
        (double)(waiting_sum / (long double)calculated.process_count);
    calculated.average_response =
        (double)(response_sum / (long double)calculated.process_count);
    calculated.context_switches = count_context_switches(&simulation->timeline);
    metrics_report_destroy(report);
    *report = calculated;
    return true;
}
