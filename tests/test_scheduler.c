#include "scheduler.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "FALHOU: %s (linha %d)\n", message, __LINE__); \
        failures++; \
    } \
} while (0)

static SimulationResult run(const ProcessSpec *items, size_t count, Algorithm algorithm,
                            int quantum, int aging, uint32_t seed) {
    SchedulerConfig config = {quantum, aging, seed};
    SimulationResult result;
    char error[SCHEDULER_ERROR_SIZE];
    if (!simulate(items, count, &config, algorithm, &result, error, sizeof(error))) {
        fprintf(stderr, "simulação falhou: %s\n", error);
        exit(2);
    }
    return result;
}

static void expect_timeline(const SimulationResult *result, const char *expected) {
    size_t index, length = strlen(expected);
    CHECK(result->timeline_length == length, "tamanho da timeline");
    if (result->timeline_length != length) return;
    for (index = 0; index < length; index++) {
        int wanted = expected[index] == '.' ? -1 : expected[index] - '1';
        if (result->timeline[index] != wanted) {
            fprintf(stderr, "FALHOU: timeline em t=%zu: obtido %d, esperado %d\n",
                    index, result->timeline[index], wanted);
            failures++;
        }
    }
}

static void test_single_process_all_algorithms(void) {
    ProcessSpec items[] = {{1, 0, 3, 2}};
    int algorithm;
    for (algorithm = 0; algorithm < ALG_COUNT; algorithm++) {
        SimulationResult result = run(items, 1, (Algorithm)algorithm, 2, 1, 42);
        expect_timeline(&result, "111");
        CHECK(result.average_turnaround == 3.0, "turnaround de processo único");
        CHECK(result.average_waiting == 0.0, "espera de processo único");
        CHECK(result.average_response == 0.0, "resposta de processo único");
        CHECK(result.context_switches == 0, "trocas de processo único");
        free_result(&result);
    }
}

static void test_selection_algorithms(void) {
    ProcessSpec shortest[] = {{1, 0, 5, 1}, {2, 0, 2, 1}, {3, 1, 1, 1}};
    ProcessSpec priority[] = {{1, 0, 3, 2}, {2, 1, 1, 1}};
    SimulationResult result = run(shortest, 3, ALG_SJF, 2, 1, 42);
    expect_timeline(&result, "22311111");
    free_result(&result);

    result = run(shortest, 3, ALG_SRTF, 2, 1, 42);
    expect_timeline(&result, "22311111");
    free_result(&result);

    result = run(priority, 2, ALG_PRIORITY_NP, 2, 1, 42);
    expect_timeline(&result, "1112");
    free_result(&result);

    result = run(priority, 2, ALG_PRIORITY_P, 2, 1, 42);
    expect_timeline(&result, "1211");
    free_result(&result);
}

static void test_fcfs_idle_and_unordered(void) {
    ProcessSpec unordered[] = {{1, 2, 1, 1}, {2, 0, 2, 1}};
    ProcessSpec idle[] = {{1, 0, 1, 1}, {2, 2, 1, 1}};
    SimulationResult result = run(unordered, 2, ALG_FCFS, 2, 1, 7);
    expect_timeline(&result, "221");
    CHECK(result.metrics[0].first_dispatch == 2, "ID preservado com chegada desordenada");
    free_result(&result);

    result = run(idle, 2, ALG_FCFS, 2, 1, 7);
    expect_timeline(&result, "1.2");
    CHECK(result.context_switches == 0, "ociosidade não conta troca");
    free_result(&result);
}

static void test_round_robin(void) {
    ProcessSpec items[] = {{1, 0, 5, 1}, {2, 1, 2, 1}};
    ProcessSpec early_finish[] = {{1, 0, 1, 1}, {2, 0, 3, 1}};
    SimulationResult result = run(items, 2, ALG_RR, 2, 1, 42);
    expect_timeline(&result, "1122111");
    CHECK(result.context_switches == 2, "trocas no Round-Robin");
    free_result(&result);

    result = run(early_finish, 2, ALG_RR, 3, 1, 42);
    expect_timeline(&result, "1222");
    free_result(&result);
}

static void test_priority_rr_aging(void) {
    ProcessSpec items[] = {{1, 0, 8, 1}, {2, 0, 2, 5}};
    SimulationResult result = run(items, 2, ALG_PRIORITY_RR, 2, 2, 42);
    expect_timeline(&result, "1111221111");
    CHECK(result.metrics[1].first_dispatch == 4, "aging promove processo desfavorecido");
    CHECK(result.timeline[0] == result.timeline[1] && result.timeline[2] == result.timeline[3],
          "prioridade não interrompe no meio do quantum");
    free_result(&result);
}

static void test_metrics(void) {
    ProcessSpec items[] = {{1, 0, 3, 1}, {2, 1, 2, 1}};
    SimulationResult result = run(items, 2, ALG_FCFS, 2, 1, 42);
    expect_timeline(&result, "11122");
    CHECK(fabs(result.average_turnaround - 3.5) < 0.0001, "turnaround médio");
    CHECK(fabs(result.average_waiting - 1.0) < 0.0001, "espera média");
    CHECK(fabs(result.average_response - 1.0) < 0.0001, "resposta média");
    CHECK(result.context_switches == 1, "troca FCFS");
    free_result(&result);
}

static void test_reproducible_tie(void) {
    ProcessSpec items[] = {{1, 0, 2, 1}, {2, 0, 2, 1}, {3, 0, 2, 1}};
    SimulationResult first = run(items, 3, ALG_SJF, 2, 1, 1234);
    SimulationResult second = run(items, 3, ALG_SJF, 2, 1, 1234);
    CHECK(first.timeline_length == second.timeline_length, "tamanho reproduzível");
    CHECK(memcmp(first.timeline, second.timeline, first.timeline_length * sizeof(int)) == 0,
          "desempate reproduzível pela semente");
    free_result(&first);
    free_result(&second);
}

static void test_process_parser(void) {
    FILE *input = tmpfile();
    ProcessSpec *items = NULL;
    size_t count = 0;
    char error[SCHEDULER_ERROR_SIZE];
    CHECK(input != NULL, "tmpfile disponível");
    if (!input) return;
    fputs("3 2 1\n0 1 4\n", input);
    rewind(input);
    CHECK(read_processes(input, &items, &count, error, sizeof(error)), "parser aceita chegadas desordenadas");
    CHECK(count == 2 && items[0].id == 1 && items[0].arrival == 3, "ordem original define IDs");
    free(items);
    fclose(input);

    input = tmpfile();
    fputs("0 0 1\n", input);
    rewind(input);
    CHECK(!read_processes(input, &items, &count, error, sizeof(error)), "parser rejeita duração zero");
    fclose(input);
}

int main(void) {
    test_single_process_all_algorithms();
    test_selection_algorithms();
    test_fcfs_idle_and_unordered();
    test_round_robin();
    test_priority_rr_aging();
    test_metrics();
    test_reproducible_tie();
    test_process_parser();
    if (failures) {
        fprintf(stderr, "%d teste(s) falharam\n", failures);
        return 1;
    }
    puts("Todos os testes unitários passaram.");
    return 0;
}
