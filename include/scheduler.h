#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Contrato público compartilhado pela CLI, pelo motor e pelos serializadores. */
#define SCHEDULER_MAX_PROCESSES 10000
#define SCHEDULER_ERROR_SIZE 256

/* Dados imutáveis de entrada. O ID segue a posição original no stdin. */
typedef struct {
    int id;
    int arrival;
    int burst;
    int priority;
} ProcessSpec;

/* Parâmetros comuns a todas as políticas de escalonamento. */
typedef struct {
    int quantum;
    int aging;
    uint32_t seed;
    bool trace;
} SchedulerConfig;

/* A ordem também define a ordem usada quando a CLI executa "all". */
typedef enum {
    ALG_FCFS,
    ALG_SJF,
    ALG_SRTF,
    ALG_PRIORITY_NP,
    ALG_PRIORITY_P,
    ALG_RR,
    ALG_PRIORITY_RR,
    ALG_COUNT
} Algorithm;

/* Instantes e métricas derivados depois que a timeline termina. */
typedef struct {
    int first_dispatch;
    int completion;
    int turnaround;
    int waiting;
    int response;
} ProcessMetrics;

/* Motivo da transição observada no início de um segundo simulado. */
typedef enum {
    DECISION_IDLE,
    DECISION_DISPATCH,
    DECISION_CONTINUE,
    DECISION_PREEMPT,
    DECISION_QUANTUM
} DecisionReason;

/* Critério que resolveu a seleção entre os processos candidatos. */
typedef enum {
    CHOICE_NONE,
    CHOICE_CRITERION,
    CHOICE_CURRENT,
    CHOICE_REMAINING,
    CHOICE_RANDOM,
    CHOICE_FIFO
} DecisionChoice;

/* Estado de um candidato imediatamente antes da execução de [t, t + 1). */
typedef struct {
    int process_index;
    int remaining;
    int effective_priority;
    int ready_wait;
    uint64_t ready_order;
} DecisionCandidate;

/* Fotografia opcional de uma decisão, usada pela visualização web. */
typedef struct {
    int cpu_before;
    int selected;
    int returned;
    int quantum_used;
    bool completes;
    bool quantum_expires;
    DecisionReason reason;
    DecisionChoice choice;
    DecisionCandidate *candidates;
    size_t candidate_count;
} DecisionSnapshot;

/* Resultado autocontido de um algoritmo para uma carga de processos. */
typedef struct {
    Algorithm algorithm;
    ProcessMetrics *metrics;
    int *timeline;
    size_t timeline_length;
    DecisionSnapshot *decisions;
    size_t decision_length;
    double average_turnaround;
    double average_waiting;
    double average_response;
    int context_switches;
} SimulationResult;

const char *algorithm_key(Algorithm algorithm);
const char *algorithm_label(Algorithm algorithm);
bool parse_algorithm(const char *text, Algorithm *algorithm);

bool read_config(const char *path, SchedulerConfig *config, char *error, size_t error_size);
bool read_processes(FILE *stream, ProcessSpec **processes, size_t *count,
                    char *error, size_t error_size);

bool simulate(const ProcessSpec *processes, size_t count, const SchedulerConfig *config,
              Algorithm algorithm, SimulationResult *result,
              char *error, size_t error_size);
void free_result(SimulationResult *result);

void print_text_result(FILE *stream, const ProcessSpec *processes, size_t count,
                       const SimulationResult *result);
void print_json(FILE *stream, const ProcessSpec *processes, size_t count,
                const SchedulerConfig *config, const SimulationResult *results,
                size_t result_count);

#endif

