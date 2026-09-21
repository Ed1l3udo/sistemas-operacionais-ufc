#include "scheduler.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { FORMAT_TEXT, FORMAT_JSON } OutputFormat;

static void usage(FILE *stream, const char *program) {
    fprintf(stream,
            "Uso: %s --config ARQUIVO [--algorithm NOME|all] [--format text|json] [--seed N]\n"
            "Algoritmos: fcfs, sjf, srtf, priority-np, priority-p, rr, priority-rr, all\n",
            program);
}

static bool parse_seed(const char *text, uint32_t *seed) {
    char *end = NULL;
    unsigned long value;
    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno || end == text || *end != '\0' || value > UINT32_MAX) return false;
    *seed = (uint32_t)value;
    return true;
}

int main(int argc, char **argv) {
    const char *config_path = NULL;
    const char *algorithm_text = "all";
    SchedulerConfig config = {.quantum = 0, .aging = 0, .seed = 42};
    OutputFormat format = FORMAT_TEXT;
    ProcessSpec *processes = NULL;
    SimulationResult results[ALG_COUNT] = {0};
    size_t process_count = 0, result_count = 0, index;
    char error[SCHEDULER_ERROR_SIZE] = {0};
    int argument;

    for (argument = 1; argument < argc; argument++) {
        if (strcmp(argv[argument], "--help") == 0 || strcmp(argv[argument], "-h") == 0) {
            usage(stdout, argv[0]);
            return 0;
        }
        if (argument + 1 >= argc) {
            fprintf(stderr, "erro: falta valor para '%s'\n", argv[argument]);
            usage(stderr, argv[0]);
            return 2;
        }
        if (strcmp(argv[argument], "--config") == 0) {
            config_path = argv[++argument];
        } else if (strcmp(argv[argument], "--algorithm") == 0) {
            algorithm_text = argv[++argument];
        } else if (strcmp(argv[argument], "--format") == 0) {
            const char *value = argv[++argument];
            if (strcmp(value, "text") == 0) format = FORMAT_TEXT;
            else if (strcmp(value, "json") == 0) format = FORMAT_JSON;
            else {
                fprintf(stderr, "erro: formato desconhecido '%s'\n", value);
                return 2;
            }
        } else if (strcmp(argv[argument], "--seed") == 0) {
            const char *value = argv[++argument];
            if (!parse_seed(value, &config.seed)) {
                fprintf(stderr, "erro: semente inválida '%s'\n", value);
                return 2;
            }
        } else {
            fprintf(stderr, "erro: opção desconhecida '%s'\n", argv[argument]);
            usage(stderr, argv[0]);
            return 2;
        }
    }
    if (!config_path) {
        fputs("erro: --config é obrigatório\n", stderr);
        usage(stderr, argv[0]);
        return 2;
    }
    if (strcmp(algorithm_text, "all") != 0) {
        Algorithm selected;
        if (!parse_algorithm(algorithm_text, &selected)) {
            fprintf(stderr, "erro: algoritmo desconhecido '%s'\n", algorithm_text);
            return 2;
        }
    }
    if (!read_config(config_path, &config, error, sizeof(error)) ||
        !read_processes(stdin, &processes, &process_count, error, sizeof(error))) {
        fprintf(stderr, "erro: %s\n", error);
        free(processes);
        return 1;
    }
    for (index = 0; index < ALG_COUNT; index++) {
        Algorithm algorithm = (Algorithm)index;
        if (strcmp(algorithm_text, "all") != 0 && strcmp(algorithm_text, algorithm_key(algorithm)) != 0) continue;
        if (!simulate(processes, process_count, &config, algorithm, &results[result_count],
                      error, sizeof(error))) {
            fprintf(stderr, "erro: %s\n", error);
            goto fail;
        }
        result_count++;
    }
    if (format == FORMAT_JSON) {
        print_json(stdout, processes, process_count, &config, results, result_count);
    } else {
        for (index = 0; index < result_count; index++) {
            print_text_result(stdout, processes, process_count, &results[index]);
        }
    }
    for (index = 0; index < result_count; index++) free_result(&results[index]);
    free(processes);
    return 0;

fail:
    for (index = 0; index < result_count; index++) free_result(&results[index]);
    free(processes);
    return 1;
}
