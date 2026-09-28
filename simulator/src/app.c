#include "app.h"

#include <inttypes.h>
#include <stdint.h>
#include <time.h>

#include "cli.h"
#include "config.h"
#include "metrics.h"
#include "output.h"
#include "process.h"
#include "scheduler.h"

typedef bool (*SchedulerFunction)(const ProcessList *, uint64_t, Simulation *,
                                  SchedulerError *);

static uint64_t automatic_seed(void)
{
    /* C17's wall and CPU clocks provide a portable, non-reproducible default. */
    return ((uint64_t)time(NULL) << 32) ^ (uint64_t)clock() ^
           UINT64_C(0x9e3779b97f4a7c15);
}

static bool run_algorithm(AlgorithmType algorithm, const ProcessList *processes,
                          const Config *config, uint64_t seed,
                          Simulation *simulation, SchedulerError *error)
{
    static const SchedulerFunction functions[] = {
        NULL, scheduler_run_fcfs, scheduler_run_sjf, scheduler_run_srtf,
        scheduler_run_priority_non_preemptive, scheduler_run_priority_preemptive,
        NULL, NULL
    };

    if (algorithm == ALGORITHM_RR) {
        return scheduler_run_round_robin(processes, config->quantum, simulation,
                                         error);
    }
    if (algorithm == ALGORITHM_PRIORITY_RR) {
        return scheduler_run_priority_round_robin(processes, config->quantum,
                                                   config->aging, simulation,
                                                   error);
    }
    if (algorithm <= ALGORITHM_ALL || algorithm > ALGORITHM_PRIORITY_RR ||
        functions[algorithm] == NULL) {
        error_set(error, EXIT_CODE_INTERNAL, "invalid scheduling algorithm");
        return false;
    }
    return functions[algorithm](processes, seed, simulation, error);
}

static bool execute_and_print(AlgorithmType algorithm,
                              const ProcessList *processes,
                              const Config *config, uint64_t seed, FILE *output,
                              SchedulerError *error)
{
    Simulation simulation = {0};
    MetricsReport report;
    bool success;

    metrics_report_init(&report);
    success = run_algorithm(algorithm, processes, config, seed, &simulation, error);
    if (success) {
        success = metrics_calculate(&simulation, &report, error);
    }
    if (success) {
        success = output_print_report(output, algorithm, config, &simulation,
                                      &report, error);
    }
    metrics_report_destroy(&report);
    simulation_destroy(&simulation);
    return success;
}

int scheduler_run_application(int argc, char *argv[], FILE *input,
                              FILE *output, FILE *errors)
{
    CliOptions options;
    Config config;
    ProcessList processes;
    SchedulerError error;
    CliParseResult cli_result;
    uint64_t seed;
    AlgorithmType algorithm;
    int exit_code = EXIT_CODE_SUCCESS;

    if (input == NULL || output == NULL || errors == NULL) {
        return EXIT_CODE_INTERNAL;
    }
    error_clear(&error);
    cli_result = cli_parse(argc, argv, &options, &error);
    if (cli_result == CLI_PARSE_HELP) {
        cli_print_usage(output, argv[0]);
        return EXIT_CODE_SUCCESS;
    }
    if (cli_result == CLI_PARSE_ERROR) {
        fprintf(errors, "scheduler: %s\n", error.message);
        cli_print_usage(errors, argv[0]);
        return error.code;
    }
    if (!config_load_file(options.config_path, &config, &error)) {
        fprintf(errors, "scheduler: %s\n", error.message);
        return error.code;
    }
    process_list_init(&processes);
    if (!process_list_read(input, &processes, &error)) {
        fprintf(errors, "scheduler: %s\n", error.message);
        exit_code = error.code;
        goto cleanup;
    }
    seed = options.seed_provided ? options.seed : automatic_seed();
    if (fprintf(output, "Seed: %" PRIu64 "\n\n", seed) < 0) {
        error_set(&error, EXIT_CODE_INTERNAL, "unable to write scheduler output");
        fprintf(errors, "scheduler: %s\n", error.message);
        exit_code = error.code;
        goto cleanup;
    }
    if (options.algorithm == ALGORITHM_ALL) {
        for (algorithm = ALGORITHM_FCFS; algorithm <= ALGORITHM_PRIORITY_RR;
             ++algorithm) {
            if (!execute_and_print(algorithm, &processes, &config, seed, output,
                                   &error)) {
                fprintf(errors, "scheduler: %s\n", error.message);
                exit_code = error.code;
                goto cleanup;
            }
            if (algorithm != ALGORITHM_PRIORITY_RR && fputc('\n', output) == EOF) {
                error_set(&error, EXIT_CODE_INTERNAL,
                          "unable to write scheduler output");
                fprintf(errors, "scheduler: %s\n", error.message);
                exit_code = error.code;
                goto cleanup;
            }
        }
    } else if (!execute_and_print(options.algorithm, &processes, &config, seed,
                                  output, &error)) {
        fprintf(errors, "scheduler: %s\n", error.message);
        exit_code = error.code;
    }

cleanup:
    process_list_destroy(&processes);
    return exit_code;
}
