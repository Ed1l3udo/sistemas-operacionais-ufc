#include "scheduler.h"

#include <stdio.h>

/* Serialização sem lógica de escalonamento: todo valor vem de SimulationResult. */

static const char *decision_reason_key(DecisionReason reason) {
    static const char *keys[] = {"idle", "dispatch", "continue", "preempt", "quantum"};
    return reason >= DECISION_IDLE && reason <= DECISION_QUANTUM ? keys[reason] : "idle";
}

static const char *decision_choice_key(DecisionChoice choice) {
    static const char *keys[] = {"none", "criterion", "current", "remaining", "random", "fifo"};
    return choice >= CHOICE_NONE && choice <= CHOICE_FIFO ? keys[choice] : "none";
}

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
    /* Diagrama vertical pedido no enunciado: uma linha para cada segundo. */
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
                              const SchedulerConfig *config, const SimulationResult *result) {
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
    fputc(']', stream);
    /* Decisions só existe quando --trace foi solicitado para uma única política. */
    if (result->decisions) {
        fputs(",\"decisions\":[", stream);
        for (index = 0; index < result->decision_length; index++) {
            const DecisionSnapshot *decision = &result->decisions[index];
            size_t candidate_index;
            if (index) fputc(',', stream);
            fprintf(stream,
                    "{\"time\":%zu,\"cpuBefore\":", index);
            if (decision->cpu_before < 0) fputs("null", stream);
            else fprintf(stream, "\"P%d\"", processes[decision->cpu_before].id);
            fputs(",\"selected\":", stream);
            if (decision->selected < 0) fputs("null", stream);
            else fprintf(stream, "\"P%d\"", processes[decision->selected].id);
            fputs(",\"returned\":", stream);
            if (decision->returned < 0) fputs("null", stream);
            else fprintf(stream, "\"P%d\"", processes[decision->returned].id);
            fprintf(stream,
                    ",\"reason\":\"%s\",\"choice\":\"%s\","
                    "\"quantumUsed\":%d,\"quantumLimit\":%d,"
                    "\"completes\":%s,\"quantumExpires\":%s,\"ready\":[",
                    decision_reason_key(decision->reason), decision_choice_key(decision->choice),
                    decision->quantum_used, config->quantum,
                    decision->completes ? "true" : "false",
                    decision->quantum_expires ? "true" : "false");
            for (candidate_index = 0; candidate_index < decision->candidate_count;
                 candidate_index++) {
                const DecisionCandidate *candidate = &decision->candidates[candidate_index];
                if (candidate_index) fputc(',', stream);
                fprintf(stream,
                        "{\"id\":\"P%d\",\"remaining\":%d,\"priority\":%d,"
                        "\"effectivePriority\":%d,\"readyWait\":%d,\"readyOrder\":%llu}",
                        processes[candidate->process_index].id, candidate->remaining,
                        processes[candidate->process_index].priority,
                        candidate->effective_priority, candidate->ready_wait,
                        (unsigned long long)candidate->ready_order);
            }
            fputs("]}", stream);
        }
        fputc(']', stream);
    }
    fputc('}', stream);
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
        print_json_result(stream, processes, count, config, &results[index]);
    }
    fputs("]}\n", stream);
}

