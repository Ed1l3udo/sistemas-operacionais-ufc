#include "scheduler.h"

#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TIMELINE_SECONDS 10000000

typedef enum { STATE_NEW, STATE_READY, STATE_RUNNING, STATE_DONE } State;

typedef struct {
    int remaining;
    int first_dispatch;
    int completion;
    int ready_wait;
    int effective_priority;
    uint64_t ready_order;
    State state;
} Runtime;

typedef struct {
    int *items;
    size_t capacity;
    size_t head;
    size_t length;
} Queue;

static void set_error(char *error, size_t size, const char *format, ...) {
    va_list args;
    if (size == 0) return;
    va_start(args, format);
    vsnprintf(error, size, format, args);
    va_end(args);
}

static uint32_t next_random(uint32_t *state) {
    uint32_t value = *state;
    if (value == 0) value = 0x6d2b79f5U;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

const char *algorithm_key(Algorithm algorithm) {
    static const char *keys[ALG_COUNT] = {
        "fcfs", "sjf", "srtf", "priority-np", "priority-p", "rr", "priority-rr"
    };
    return algorithm >= 0 && algorithm < ALG_COUNT ? keys[algorithm] : "unknown";
}

const char *algorithm_label(Algorithm algorithm) {
    static const char *labels[ALG_COUNT] = {
        "FCFS", "SJF (não preemptivo)", "SRTF", "Prioridade (não preemptivo)",
        "Prioridade (preemptivo)", "Round-Robin", "Round-Robin com prioridade e envelhecimento"
    };
    return algorithm >= 0 && algorithm < ALG_COUNT ? labels[algorithm] : "Desconhecido";
}

bool parse_algorithm(const char *text, Algorithm *algorithm) {
    int index;
    for (index = 0; index < ALG_COUNT; index++) {
        if (strcmp(text, algorithm_key((Algorithm)index)) == 0) {
            *algorithm = (Algorithm)index;
            return true;
        }
    }
    return false;
}

static bool prepare(const ProcessSpec *processes, size_t count, Runtime **runtime,
                    SimulationResult *result, char *error, size_t error_size) {
    size_t index;
    long long maximum = 0, bursts = 0;

    for (index = 0; index < count; index++) {
        if (processes[index].arrival > maximum) maximum = processes[index].arrival;
        bursts += processes[index].burst;
    }
    maximum += bursts;
    if (maximum <= 0 || maximum > MAX_TIMELINE_SECONDS || maximum > INT_MAX) {
        set_error(error, error_size, "simulação excede o limite de %d segundos", MAX_TIMELINE_SECONDS);
        return false;
    }
    *runtime = calloc(count, sizeof(**runtime));
    result->metrics = calloc(count, sizeof(*result->metrics));
    result->timeline = malloc((size_t)maximum * sizeof(*result->timeline));
    if (!*runtime || !result->metrics || !result->timeline) {
        set_error(error, error_size, "memória insuficiente para executar a simulação");
        free(*runtime);
        free(result->metrics);
        free(result->timeline);
        *runtime = NULL;
        result->metrics = NULL;
        result->timeline = NULL;
        return false;
    }
    for (index = 0; index < count; index++) {
        (*runtime)[index].remaining = processes[index].burst;
        (*runtime)[index].first_dispatch = -1;
        (*runtime)[index].completion = -1;
        (*runtime)[index].effective_priority = processes[index].priority;
        (*runtime)[index].state = STATE_NEW;
    }
    return true;
}

static void admit_all(const ProcessSpec *processes, Runtime *runtime, size_t count, int time) {
    size_t index;
    for (index = 0; index < count; index++) {
        if (runtime[index].state == STATE_NEW && processes[index].arrival <= time) {
            runtime[index].state = STATE_READY;
        }
    }
}

static int primary_value(const ProcessSpec *processes, const Runtime *runtime,
                         int index, Algorithm algorithm) {
    switch (algorithm) {
        case ALG_FCFS: return processes[index].arrival;
        case ALG_SJF:
        case ALG_SRTF: return runtime[index].remaining;
        case ALG_PRIORITY_NP:
        case ALG_PRIORITY_P: return processes[index].priority;
        default: return INT_MAX;
    }
}

static int choose_selected(const ProcessSpec *processes, const Runtime *runtime, size_t count,
                           Algorithm algorithm, int current, uint32_t *random_state) {
    size_t index;
    int best_primary = INT_MAX;
    int best_remaining = INT_MAX;
    int candidates = 0;
    int selected = -1;

    for (index = 0; index < count; index++) {
        int value;
        if (runtime[index].state != STATE_READY && runtime[index].state != STATE_RUNNING) continue;
        value = primary_value(processes, runtime, (int)index, algorithm);
        if (value < best_primary) best_primary = value;
    }
    if (current >= 0 && runtime[current].state != STATE_DONE &&
        primary_value(processes, runtime, current, algorithm) == best_primary) {
        return current;
    }
    for (index = 0; index < count; index++) {
        if (runtime[index].state != STATE_READY) continue;
        if (primary_value(processes, runtime, (int)index, algorithm) != best_primary) continue;
        if (runtime[index].remaining < best_remaining) best_remaining = runtime[index].remaining;
    }
    for (index = 0; index < count; index++) {
        if (runtime[index].state != STATE_READY || runtime[index].remaining != best_remaining) continue;
        if (primary_value(processes, runtime, (int)index, algorithm) != best_primary) continue;
        candidates++;
        if ((int)(next_random(random_state) % (uint32_t)candidates) == 0) selected = (int)index;
    }
    return selected;
}

static void run_second(Runtime *runtime, int selected, int time, int *timeline) {
    if (selected < 0) {
        timeline[time] = -1;
        return;
    }
    runtime[selected].state = STATE_RUNNING;
    if (runtime[selected].first_dispatch < 0) runtime[selected].first_dispatch = time;
    timeline[time] = selected;
    runtime[selected].remaining--;
    if (runtime[selected].remaining == 0) {
        runtime[selected].state = STATE_DONE;
        runtime[selected].completion = time + 1;
    }
}

static bool is_preemptive(Algorithm algorithm) {
    return algorithm == ALG_SRTF || algorithm == ALG_PRIORITY_P;
}

static void simulate_selected(const ProcessSpec *processes, size_t count, Runtime *runtime,
                              const SchedulerConfig *config, Algorithm algorithm,
                              SimulationResult *result) {
    size_t completed = 0;
    int time = 0, current = -1;
    uint32_t random_state = config->seed;

    while (completed < count) {
        int selected;
        admit_all(processes, runtime, count, time);
        if (current >= 0 && runtime[current].state == STATE_RUNNING && !is_preemptive(algorithm)) {
            selected = current;
        } else {
            if (current >= 0 && runtime[current].state == STATE_RUNNING) runtime[current].state = STATE_READY;
            selected = choose_selected(processes, runtime, count, algorithm, current, &random_state);
        }
        run_second(runtime, selected, time, result->timeline);
        if (selected >= 0 && runtime[selected].state == STATE_DONE) {
            completed++;
            current = -1;
        } else {
            current = selected;
        }
        time++;
    }
    result->timeline_length = (size_t)time;
}

static bool queue_init(Queue *queue, size_t count) {
    queue->capacity = count + 1;
    queue->items = malloc(queue->capacity * sizeof(*queue->items));
    queue->head = queue->length = 0;
    return queue->items != NULL;
}

static void queue_push(Queue *queue, int value) {
    size_t position = (queue->head + queue->length) % queue->capacity;
    queue->items[position] = value;
    queue->length++;
}

static int queue_pop(Queue *queue) {
    int value = queue->items[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    queue->length--;
    return value;
}

static void admit_fifo(const ProcessSpec *processes, Runtime *runtime, size_t count,
                       int time, Queue *queue) {
    size_t index;
    for (index = 0; index < count; index++) {
        if (runtime[index].state == STATE_NEW && processes[index].arrival <= time) {
            runtime[index].state = STATE_READY;
            queue_push(queue, (int)index);
        }
    }
}

static bool simulate_rr(const ProcessSpec *processes, size_t count, Runtime *runtime,
                        const SchedulerConfig *config, SimulationResult *result,
                        char *error, size_t error_size) {
    Queue queue;
    size_t completed = 0;
    int time = 0, current = -1, quantum_used = 0, pending = -1;
    if (!queue_init(&queue, count)) {
        set_error(error, error_size, "memória insuficiente para a fila Round-Robin");
        return false;
    }
    while (completed < count) {
        admit_fifo(processes, runtime, count, time, &queue);
        if (pending >= 0) {
            runtime[pending].state = STATE_READY;
            queue_push(&queue, pending);
            pending = -1;
        }
        if (current < 0 && queue.length > 0) {
            current = queue_pop(&queue);
            quantum_used = 0;
        }
        run_second(runtime, current, time, result->timeline);
        if (current >= 0) {
            quantum_used++;
            if (runtime[current].state == STATE_DONE) {
                completed++;
                current = -1;
            } else if (quantum_used == config->quantum) {
                pending = current;
                current = -1;
            }
        }
        time++;
    }
    result->timeline_length = (size_t)time;
    free(queue.items);
    return true;
}

static void admit_priority_fifo(const ProcessSpec *processes, Runtime *runtime, size_t count,
                                int time, uint64_t *order) {
    size_t index;
    for (index = 0; index < count; index++) {
        if (runtime[index].state == STATE_NEW && processes[index].arrival <= time) {
            runtime[index].state = STATE_READY;
            runtime[index].ready_order = (*order)++;
        }
    }
}

static int choose_priority_rr(const Runtime *runtime, size_t count) {
    size_t index;
    int selected = -1;
    for (index = 0; index < count; index++) {
        if (runtime[index].state != STATE_READY) continue;
        if (selected < 0 || runtime[index].effective_priority < runtime[selected].effective_priority ||
            (runtime[index].effective_priority == runtime[selected].effective_priority &&
             runtime[index].ready_order < runtime[selected].ready_order)) {
            selected = (int)index;
        }
    }
    return selected;
}

static void age_waiting(const ProcessSpec *processes, Runtime *runtime, size_t count,
                        const SchedulerConfig *config, int selected) {
    size_t index;
    for (index = 0; index < count; index++) {
        int periods, effective;
        if ((int)index == selected || runtime[index].state != STATE_READY) continue;
        runtime[index].ready_wait++;
        periods = runtime[index].ready_wait / config->quantum;
        effective = processes[index].priority - config->aging * periods;
        runtime[index].effective_priority = effective < 1 ? 1 : effective;
    }
}

static void simulate_priority_rr(const ProcessSpec *processes, size_t count, Runtime *runtime,
                                 const SchedulerConfig *config, SimulationResult *result) {
    size_t completed = 0;
    int time = 0, current = -1, quantum_used = 0, pending = -1;
    uint64_t order = 0;
    while (completed < count) {
        admit_priority_fifo(processes, runtime, count, time, &order);
        if (pending >= 0) {
            runtime[pending].state = STATE_READY;
            runtime[pending].ready_order = order++;
            pending = -1;
        }
        if (current < 0) {
            current = choose_priority_rr(runtime, count);
            quantum_used = 0;
            if (current >= 0) {
                runtime[current].ready_wait = 0;
                runtime[current].effective_priority = processes[current].priority;
            }
        }
        run_second(runtime, current, time, result->timeline);
        age_waiting(processes, runtime, count, config, current);
        if (current >= 0) {
            quantum_used++;
            if (runtime[current].state == STATE_DONE) {
                completed++;
                current = -1;
            } else if (quantum_used == config->quantum) {
                pending = current;
                current = -1;
            }
        }
        time++;
    }
    result->timeline_length = (size_t)time;
}

static void calculate_metrics(const ProcessSpec *processes, size_t count,
                              const Runtime *runtime, SimulationResult *result) {
    size_t index;
    double turnaround = 0, waiting = 0, response = 0;
    int switches = 0;
    for (index = 0; index < count; index++) {
        ProcessMetrics *metric = &result->metrics[index];
        metric->first_dispatch = runtime[index].first_dispatch;
        metric->completion = runtime[index].completion;
        metric->turnaround = metric->completion - processes[index].arrival;
        metric->waiting = metric->turnaround - processes[index].burst;
        metric->response = metric->first_dispatch - processes[index].arrival;
        turnaround += metric->turnaround;
        waiting += metric->waiting;
        response += metric->response;
    }
    for (index = 1; index < result->timeline_length; index++) {
        if (result->timeline[index - 1] >= 0 && result->timeline[index] >= 0 &&
            result->timeline[index - 1] != result->timeline[index]) switches++;
    }
    result->average_turnaround = turnaround / (double)count;
    result->average_waiting = waiting / (double)count;
    result->average_response = response / (double)count;
    result->context_switches = switches;
}

bool simulate(const ProcessSpec *processes, size_t count, const SchedulerConfig *config,
              Algorithm algorithm, SimulationResult *result,
              char *error, size_t error_size) {
    Runtime *runtime = NULL;
    bool ok = true;
    if (!processes || count == 0 || !config || !result || algorithm < 0 || algorithm >= ALG_COUNT) {
        set_error(error, error_size, "parâmetros inválidos para a simulação");
        return false;
    }
    memset(result, 0, sizeof(*result));
    result->algorithm = algorithm;
    if (!prepare(processes, count, &runtime, result, error, error_size)) return false;
    if (algorithm == ALG_RR) {
        ok = simulate_rr(processes, count, runtime, config, result, error, error_size);
    } else if (algorithm == ALG_PRIORITY_RR) {
        simulate_priority_rr(processes, count, runtime, config, result);
    } else {
        simulate_selected(processes, count, runtime, config, algorithm, result);
    }
    if (ok) calculate_metrics(processes, count, runtime, result);
    free(runtime);
    if (!ok) free_result(result);
    return ok;
}

void free_result(SimulationResult *result) {
    if (!result) return;
    free(result->metrics);
    free(result->timeline);
    memset(result, 0, sizeof(*result));
}
