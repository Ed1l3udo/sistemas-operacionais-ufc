#include "scheduler.h"

#include <stdio.h>

void print_text_result(FILE *stream, const ProcessSpec *processes, size_t count,
                       const SimulationResult *result) {
    size_t index, time;
    fprintf(stream, "\n=== %s ===\n", algorithm_label(result->algorithm));
    fprintf(stream, "Tempo médio de vida (turnaround): %.2f s\n", result->average_turnaround);
    fprintf(stream, "Tempo médio de espera: %.2f s\n", result->average_waiting);
    fprintf(stream, "Tempo médio de resposta: %.2f s\n", result->average_response);
    fprintf(stream, "Trocas de contexto: %d\n\n", result->context_switches);
    fprintf(stream, "Processo  Chegada  Duração  Prioridade  Primeiro  Conclusão  Turnaround  Espera  Resposta\n");
    for (index = 0; index < count; index++) {
        const ProcessMetrics *metric = &result->metrics[index];
        fprintf(stream, "P%-8d %-8d %-8d %-11d %-9d %-10d %-11d %-7d %d\n",
                processes[index].id, processes[index].arrival, processes[index].burst,
                processes[index].priority, metric->first_dispatch, metric->completion,
                metric->turnaround, metric->waiting, metric->response);
    }
    fprintf(stream, "\nTempo   ");
    for (index = 0; index < count; index++) fprintf(stream, "P%-5d", processes[index].id);
    fputc('\n', stream);
    for (time = 0; time < result->timeline_length; time++) {
        fprintf(stream, "%3zu-%-3zu", time, time + 1);
        for (index = 0; index < count; index++) {
            const char *cell = "";
            if (result->timeline[time] == (int)index) cell = "##";
            else if ((int)time >= processes[index].arrival &&
                     (int)time < result->metrics[index].completion) cell = "--";
            fprintf(stream, "%-6s", cell);
        }
        fputc('\n', stream);
    }
}

static void print_json_result(FILE *stream, const ProcessSpec *processes, size_t count,
                              const SimulationResult *result) {
    size_t index;
    fprintf(stream,
            "{\"algorithm\":\"%s\",\"label\":\"%s\","
            "\"averageTurnaround\":%.6f,\"averageWaiting\":%.6f,"
            "\"averageResponse\":%.6f,\"contextSwitches\":%d,\"metrics\":[",
            algorithm_key(result->algorithm), algorithm_label(result->algorithm),
            result->average_turnaround, result->average_waiting,
            result->average_response, result->context_switches);
    for (index = 0; index < count; index++) {
        const ProcessMetrics *metric = &result->metrics[index];
        if (index) fputc(',', stream);
        fprintf(stream,
                "{\"id\":\"P%d\",\"arrival\":%d,\"burst\":%d,\"priority\":%d,"
                "\"firstDispatch\":%d,\"completion\":%d,\"turnaround\":%d,"
                "\"waiting\":%d,\"response\":%d}",
                processes[index].id, processes[index].arrival, processes[index].burst,
                processes[index].priority, metric->first_dispatch, metric->completion,
                metric->turnaround, metric->waiting, metric->response);
    }
    fputs("],\"timeline\":[", stream);
    for (index = 0; index < result->timeline_length; index++) {
        if (index) fputc(',', stream);
        if (result->timeline[index] < 0) {
            fprintf(stream, "{\"time\":%zu,\"process\":null}", index);
        } else {
            fprintf(stream, "{\"time\":%zu,\"process\":\"P%d\"}", index,
                    processes[result->timeline[index]].id);
        }
    }
    fputs("]}", stream);
}

void print_json(FILE *stream, const ProcessSpec *processes, size_t count,
                const SchedulerConfig *config, const SimulationResult *results,
                size_t result_count) {
    size_t index;
    fprintf(stream, "{\"config\":{\"quantum\":%d,\"aging\":%d,\"seed\":%u},\"processes\":[",
            config->quantum, config->aging, config->seed);
    for (index = 0; index < count; index++) {
        if (index) fputc(',', stream);
        fprintf(stream, "{\"id\":\"P%d\",\"arrival\":%d,\"burst\":%d,\"priority\":%d}",
                processes[index].id, processes[index].arrival,
                processes[index].burst, processes[index].priority);
    }
    fputs("],\"results\":[", stream);
    for (index = 0; index < result_count; index++) {
        if (index) fputc(',', stream);
        print_json_result(stream, processes, count, &results[index]);
    }
    fputs("]}\n", stream);
}

