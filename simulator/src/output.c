#include "output.h"

#include <inttypes.h>

const char *output_algorithm_name(AlgorithmType algorithm)
{
    switch (algorithm) {
    case ALGORITHM_FCFS:
        return "FCFS";
    case ALGORITHM_SJF:
        return "Shortest Job First (SJF)";
    case ALGORITHM_SRTF:
        return "Shortest Remaining Time First (SRTF)";
    case ALGORITHM_PRIORITY_NON_PREEMPTIVE:
        return "Prioridade sem preempção";
    case ALGORITHM_PRIORITY_PREEMPTIVE:
        return "Prioridade com preempção";
    case ALGORITHM_RR:
        return "Round-Robin";
    case ALGORITHM_PRIORITY_RR:
        return "Round-Robin com prioridade e aging";
    case ALGORITHM_ALL:
        break;
    }
    return "Todos os algoritmos";
}

static size_t decimal_digits(int value)
{
    size_t digits = 1;

    while (value >= 10) {
        value /= 10;
        ++digits;
    }
    return digits;
}

static size_t decimal_digits_int64(int64_t value)
{
    size_t digits = 1;

    while (value >= 10) {
        value /= 10;
        ++digits;
    }
    return digits;
}

static bool print_spaces(FILE *stream, size_t count)
{
    while (count > 0) {
        if (fputc(' ', stream) == EOF) {
            return false;
        }
        --count;
    }
    return true;
}

static size_t process_column_width(int id)
{
    size_t label_width = decimal_digits(id) + 1;

    return label_width > 2 ? label_width : 2;
}

static const char *state_for_process(const Process *process, int executed_id,
                                     int64_t time)
{
    if (time < process->arrival_time) {
        return "..";
    }
    if (process->id == executed_id) {
        return "##";
    }
    if (time >= process->completion_time) {
        return "OK";
    }
    return "--";
}

static bool print_timeline(FILE *stream, const Simulation *simulation,
                           SchedulerError *error)
{
    size_t index;
    size_t time_width = 5;
    size_t cpu_width = 4;

    for (index = 0; index < simulation->timeline.count; ++index) {
        const TimelineEntry *entry = &simulation->timeline.items[index];
        size_t interval_width = decimal_digits_int64(entry->start_time) + 1 +
                                decimal_digits_int64(entry->start_time + 1);

        if (interval_width > time_width) {
            time_width = interval_width;
        }
        if (entry->process_id != TIMELINE_IDLE_PROCESS_ID) {
            size_t process_label_width = decimal_digits(entry->process_id) + 1;

            if (process_label_width > cpu_width) {
                cpu_width = process_label_width;
            }
        }
    }

    if (fprintf(stream, "tempo") < 0 || !print_spaces(stream, time_width - 5 + 2) ||
        fprintf(stream, "CPU") < 0 || !print_spaces(stream, cpu_width - 3)) {
        goto write_error;
    }
    for (index = 0; index < simulation->processes.count; ++index) {
        const Process *process = &simulation->processes.items[index];
        size_t width = process_column_width(process->id);
        size_t label_width = decimal_digits(process->id) + 1;

        if (fprintf(stream, "  %*sP%d", (int)(width - label_width), "",
                    process->id) < 0) {
            goto write_error;
        }
    }
    if (fprintf(stream, "\n") < 0) {
        goto write_error;
    }
    for (index = 0; index < simulation->timeline.count; ++index) {
        const TimelineEntry *entry = &simulation->timeline.items[index];
        int64_t end_time = entry->start_time + 1;
        size_t interval_width = decimal_digits_int64(entry->start_time) + 1 +
                                decimal_digits_int64(end_time);
        size_t cpu_label_width;
        size_t process_index;

        if (fprintf(stream, "%" PRId64 "-%" PRId64, entry->start_time,
                    end_time) < 0 ||
            !print_spaces(stream, time_width - interval_width + 2)) {
            goto write_error;
        }
        if (entry->process_id == TIMELINE_IDLE_PROCESS_ID) {
            if (fprintf(stream, "  IDLE") < 0) {
                goto write_error;
            }
            cpu_label_width = 4;
        } else if (fprintf(stream, "  P%d", entry->process_id) < 0) {
            goto write_error;
        } else {
            cpu_label_width = decimal_digits(entry->process_id) + 1;
        }
        if (!print_spaces(stream, cpu_width - cpu_label_width)) {
            goto write_error;
        }
        for (process_index = 0; process_index < simulation->processes.count;
             ++process_index) {
            const Process *process = &simulation->processes.items[process_index];
            size_t width = process_column_width(process->id);

            if (fprintf(stream, "  %*s", (int)width,
                        state_for_process(process, entry->process_id,
                                          entry->start_time)) < 0) {
                goto write_error;
            }
        }
        if (fputc('\n', stream) == EOF) {
            goto write_error;
        }
    }
    if (fprintf(stream,
                "Legenda: ## = executando; -- = pronto e esperando; "
                ".. = ainda não chegou; OK = finalizado; IDLE = CPU ociosa\n") < 0) {
        goto write_error;
    }
    return true;

write_error:
    error_set(error, EXIT_CODE_INTERNAL, "unable to write scheduler output");
    return false;
}

bool output_print_report(FILE *stream, AlgorithmType algorithm,
                         const Config *config, const Simulation *simulation,
                         const MetricsReport *report, SchedulerError *error)
{
    if (stream == NULL || config == NULL || simulation == NULL || report == NULL ||
        report->processes == NULL ||
        report->process_count != simulation->processes.count ||
        algorithm == ALGORITHM_ALL) {
        error_set(error, EXIT_CODE_INTERNAL, "invalid scheduler output request");
        return false;
    }
    if (fprintf(stream, "=== %s ===\n", output_algorithm_name(algorithm)) < 0 ||
        fprintf(stream, "Tempo médio de vida (turnaround, tt): %.2f\n",
                report->average_turnaround) < 0 ||
        fprintf(stream, "Tempo médio de espera (waiting, tw): %.2f\n",
                report->average_waiting) < 0 ||
        fprintf(stream, "Tempo médio de resposta: %.2f\n",
                report->average_response) < 0 ||
        fprintf(stream, "Trocas de contexto: %zu\n", report->context_switches) < 0) {
        error_set(error, EXIT_CODE_INTERNAL, "unable to write scheduler output");
        return false;
    }
    if (algorithm == ALGORITHM_RR || algorithm == ALGORITHM_PRIORITY_RR) {
        if (fprintf(stream, "Quantum: %d\n", config->quantum) < 0) {
            error_set(error, EXIT_CODE_INTERNAL, "unable to write scheduler output");
            return false;
        }
    }
    if (algorithm == ALGORITHM_PRIORITY_RR &&
        fprintf(stream, "Aging: %d\n", config->aging) < 0) {
        error_set(error, EXIT_CODE_INTERNAL, "unable to write scheduler output");
        return false;
    }
    return print_timeline(stream, simulation, error);
}
