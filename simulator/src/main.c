#include <stdio.h>

#include "cli.h"
#include "config.h"
#include "process.h"

int main(int argc, char *argv[])
{
    CliOptions options;
    Config config;
    ProcessList processes;
    ProcessList process_copy;
    SchedulerError error;
    CliParseResult cli_result;
    int exit_code = EXIT_CODE_SUCCESS;

    error_clear(&error);
    cli_result = cli_parse(argc, argv, &options, &error);
    if (cli_result == CLI_PARSE_HELP) {
        cli_print_usage(stdout, argv[0]);
        return EXIT_CODE_SUCCESS;
    }
    if (cli_result == CLI_PARSE_ERROR) {
        fprintf(stderr, "scheduler: %s\n", error.message);
        cli_print_usage(stderr, argv[0]);
        return error.code;
    }

    if (!config_load_file(options.config_path, &config, &error)) {
        fprintf(stderr, "scheduler: %s\n", error.message);
        return error.code;
    }
    (void)config;

    process_list_init(&processes);
    process_list_init(&process_copy);
    if (!process_list_read(stdin, &processes, &error)) {
        fprintf(stderr, "scheduler: %s\n", error.message);
        exit_code = error.code;
        goto cleanup;
    }
    if (!process_list_clone(&processes, &process_copy, &error)) {
        fprintf(stderr, "scheduler: %s\n", error.message);
        exit_code = error.code;
        goto cleanup;
    }

cleanup:
    process_list_destroy(&process_copy);
    process_list_destroy(&processes);
    return exit_code;
}
