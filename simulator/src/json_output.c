#include "json_output.h"

#include <inttypes.h>
#include <math.h>

#include "output.h"

static bool write_failure(SchedulerError *error)
{
    error_set(error, EXIT_CODE_INTERNAL, "unable to write JSON output");
    return false;
}

static const char *algorithm_id(AlgorithmType algorithm)
{
    switch (algorithm) {
    case ALGORITHM_FCFS:
        return "fcfs";
    case ALGORITHM_SJF:
        return "sjf";
    case ALGORITHM_SRTF:
        return "srtf";
    case ALGORITHM_PRIORITY_NON_PREEMPTIVE:
        return "priority-np";
    case ALGORITHM_PRIORITY_PREEMPTIVE:
        return "priority-p";
    case ALGORITHM_RR:
        return "rr";
    case ALGORITHM_PRIORITY_RR:
        return "priority-rr";
    case ALGORITHM_ALL:
        break;
    }
    return NULL;
}

bool json_output_write_escaped_string(FILE *stream, const char *text,
                                      SchedulerError *error)
{
    const unsigned char *cursor = (const unsigned char *)text;

    if (stream == NULL || text == NULL) {
        error_set(error, EXIT_CODE_INTERNAL, "invalid JSON string request");
        return false;
    }
    if (fputc('"', stream) == EOF) {
        return write_failure(error);
    }
    while (*cursor != '\0') {
        switch (*cursor) {
        case '"':
            if (fputs("\\\"", stream) == EOF) {
                return write_failure(error);
            }
            break;
        case '\\':
            if (fputs("\\\\", stream) == EOF) {
                return write_failure(error);
            }
            break;
        case '\b':
            if (fputs("\\b", stream) == EOF) {
                return write_failure(error);
            }
            break;
        case '\f':
            if (fputs("\\f", stream) == EOF) {
                return write_failure(error);
            }
            break;
        case '\n':
            if (fputs("\\n", stream) == EOF) {
                return write_failure(error);
            }
            break;
        case '\r':
            if (fputs("\\r", stream) == EOF) {
                return write_failure(error);
            }
            break;
        case '\t':
            if (fputs("\\t", stream) == EOF) {
                return write_failure(error);
            }
            break;
        default:
            if (*cursor < 0x20) {
                if (fprintf(stream, "\\u%04x", (unsigned int)*cursor) < 0) {
                    return write_failure(error);
                }
            } else if (fputc(*cursor, stream) == EOF) {
                return write_failure(error);
            }
            break;
        }
        ++cursor;
    }
    if (fputc('"', stream) == EOF) {
        return write_failure(error);
    }
    return true;
}

bool json_output_begin_document(FILE *stream, uint64_t seed,
                                const Config *config, SchedulerError *error)
{
    if (stream == NULL || config == NULL) {
        error_set(error, EXIT_CODE_INTERNAL, "invalid JSON document request");
        return false;
    }
    if (fprintf(stream,
                "{\n  \"schemaVersion\": 1,\n  \"seed\": \"%" PRIu64
                "\",\n  \"config\": {\"quantum\": %d, \"aging\": %d},"
                "\n  \"algorithms\": [\n",
                seed, config->quantum, config->aging) < 0) {
        return write_failure(error);
    }
    return true;
}

static bool write_metrics(FILE *stream, const MetricsReport *report,
                          SchedulerError *error)
{
    if (!isfinite(report->average_turnaround) ||
        !isfinite(report->average_waiting) ||
        !isfinite(report->average_response)) {
        error_set(error, EXIT_CODE_INTERNAL, "non-finite metric for JSON output");
        return false;
    }
    if (fprintf(stream,
                "\"metrics\": {\"averageTurnaround\": %.17g, "
                "\"averageWaiting\": %.17g, \"averageResponse\": %.17g, "
                "\"contextSwitches\": %zu}",
                report->average_turnaround, report->average_waiting,
                report->average_response, report->context_switches) < 0) {
        return write_failure(error);
    }
    return true;
}

static bool write_processes(FILE *stream, const Simulation *simulation,
                            const MetricsReport *report, SchedulerError *error)
{
    size_t index;

    if (fputs(",\n      \"processes\": [", stream) == EOF) {
        return write_failure(error);
    }
    for (index = 0; index < simulation->processes.count; ++index) {
        const Process *process = &simulation->processes.items[index];
        const ProcessMetrics *metrics = &report->processes[index];

        if (process->id != metrics->process_id) {
            error_set(error, EXIT_CODE_INTERNAL,
                      "process metrics do not match simulation order");
            return false;
        }
        if (fprintf(stream,
                    "%s\n        {\"id\": %d, \"arrivalTime\": %d, "
                    "\"burstTime\": %d, \"staticPriority\": %d, "
                    "\"finalDynamicPriority\": %d, \"firstExecution\": %" PRId64
                    ", \"completionTime\": %" PRId64 ", \"turnaroundTime\": %" PRId64
                    ", \"waitingTime\": %" PRId64 ", \"responseTime\": %" PRId64 "}",
                    index == 0 ? "" : ",", process->id, process->arrival_time,
                    process->burst_time, process->static_priority,
                    process->dynamic_priority, process->first_execution,
                    process->completion_time, metrics->turnaround_time,
                    metrics->waiting_time, metrics->response_time) < 0) {
            return write_failure(error);
        }
    }
    if (fputs("\n      ]", stream) == EOF) {
        return write_failure(error);
    }
    return true;
}

static bool write_timeline(FILE *stream, const Timeline *timeline,
                           SchedulerError *error)
{
    size_t index;

    if (fputs(",\n      \"timeline\": [", stream) == EOF) {
        return write_failure(error);
    }
    for (index = 0; index < timeline->count; ++index) {
        const TimelineEntry *entry = &timeline->items[index];

        if (fprintf(stream, "%s\n        {\"start\": %" PRId64 ", \"end\": %" PRId64
                    ", \"processId\": ", index == 0 ? "" : ",",
                    entry->start_time, entry->start_time + 1) < 0) {
            return write_failure(error);
        }
        if (entry->process_id == TIMELINE_IDLE_PROCESS_ID) {
            if (fputs("null}", stream) == EOF) {
                return write_failure(error);
            }
        } else if (fprintf(stream, "%d}", entry->process_id) < 0) {
            return write_failure(error);
        }
    }
    if (fputs("\n      ]", stream) == EOF) {
        return write_failure(error);
    }
    return true;
}

bool json_output_write_algorithm(FILE *stream, AlgorithmType algorithm,
                                 const Simulation *simulation,
                                 const MetricsReport *report, bool prepend_comma,
                                 SchedulerError *error)
{
    const char *id = algorithm_id(algorithm);

    if (stream == NULL || id == NULL || simulation == NULL || report == NULL ||
        report->processes == NULL ||
        report->process_count != simulation->processes.count) {
        error_set(error, EXIT_CODE_INTERNAL, "invalid JSON algorithm request");
        return false;
    }
    if (fputs(prepend_comma ? ",\n    {\"id\": " : "    {\"id\": ", stream) == EOF ||
        !json_output_write_escaped_string(stream, id, error) ||
        fputs(", \"name\": ", stream) == EOF ||
        !json_output_write_escaped_string(stream, output_algorithm_name(algorithm),
                                          error) ||
        fputs(",\n      ", stream) == EOF ||
        !write_metrics(stream, report, error) ||
        !write_processes(stream, simulation, report, error) ||
        !write_timeline(stream, &simulation->timeline, error) ||
        fputs("\n    }", stream) == EOF) {
        if (error->code == EXIT_CODE_SUCCESS) {
            return write_failure(error);
        }
        return false;
    }
    return true;
}

bool json_output_end_document(FILE *stream, SchedulerError *error)
{
    if (stream == NULL) {
        error_set(error, EXIT_CODE_INTERNAL, "invalid JSON document request");
        return false;
    }
    if (fputs("\n  ]\n}\n", stream) == EOF) {
        return write_failure(error);
    }
    return true;
}
