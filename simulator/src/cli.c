#include "cli.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static bool parse_algorithm(const char *name, AlgorithmType *algorithm)
{
    if (strcmp(name, "all") == 0) {
        *algorithm = ALGORITHM_ALL;
    } else if (strcmp(name, "fcfs") == 0) {
        *algorithm = ALGORITHM_FCFS;
    } else if (strcmp(name, "sjf") == 0) {
        *algorithm = ALGORITHM_SJF;
    } else if (strcmp(name, "srtf") == 0) {
        *algorithm = ALGORITHM_SRTF;
    } else if (strcmp(name, "priority-np") == 0) {
        *algorithm = ALGORITHM_PRIORITY_NON_PREEMPTIVE;
    } else if (strcmp(name, "priority-p") == 0) {
        *algorithm = ALGORITHM_PRIORITY_PREEMPTIVE;
    } else if (strcmp(name, "rr") == 0) {
        *algorithm = ALGORITHM_RR;
    } else if (strcmp(name, "priority-rr") == 0) {
        *algorithm = ALGORITHM_PRIORITY_RR;
    } else {
        return false;
    }

    return true;
}

static bool parse_seed(const char *text, uint64_t *seed)
{
    char *end = NULL;
    unsigned long long parsed_seed;

    if (text[0] == '\0' || text[0] == '-') {
        return false;
    }

    errno = 0;
    parsed_seed = strtoull(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE ||
        parsed_seed > UINT64_MAX) {
        return false;
    }

    *seed = (uint64_t)parsed_seed;
    return true;
}

CliParseResult cli_parse(int argc, char *argv[], CliOptions *options,
                         SchedulerError *error)
{
    bool algorithm_seen = false;
    bool seed_seen = false;
    int index;

    if (options == NULL) {
        error_set(error, EXIT_CODE_INTERNAL, "invalid CLI options destination");
        return CLI_PARSE_ERROR;
    }
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        return CLI_PARSE_HELP;
    }
    if (argc < 2) {
        error_set(error, EXIT_CODE_USAGE, "missing configuration file");
        return CLI_PARSE_ERROR;
    }
    if (strncmp(argv[1], "--", 2) == 0) {
        error_set(error, EXIT_CODE_USAGE, "missing configuration file");
        return CLI_PARSE_ERROR;
    }

    options->config_path = argv[1];
    options->algorithm = ALGORITHM_ALL;
    options->seed_provided = false;
    options->seed = 0;

    for (index = 2; index < argc; ++index) {
        if (strcmp(argv[index], "--algorithm") == 0) {
            if (algorithm_seen) {
                error_set(error, EXIT_CODE_USAGE,
                          "--algorithm may only be provided once");
                return CLI_PARSE_ERROR;
            }
            if (++index >= argc) {
                error_set(error, EXIT_CODE_USAGE,
                          "--algorithm requires an algorithm name");
                return CLI_PARSE_ERROR;
            }
            if (!parse_algorithm(argv[index], &options->algorithm)) {
                error_set(error, EXIT_CODE_USAGE, "unknown algorithm '%s'",
                          argv[index]);
                return CLI_PARSE_ERROR;
            }
            algorithm_seen = true;
        } else if (strcmp(argv[index], "--seed") == 0) {
            if (seed_seen) {
                error_set(error, EXIT_CODE_USAGE,
                          "--seed may only be provided once");
                return CLI_PARSE_ERROR;
            }
            if (++index >= argc) {
                error_set(error, EXIT_CODE_USAGE,
                          "--seed requires a non-negative integer");
                return CLI_PARSE_ERROR;
            }
            if (!parse_seed(argv[index], &options->seed)) {
                error_set(error, EXIT_CODE_USAGE,
                          "--seed requires a uint64 non-negative integer");
                return CLI_PARSE_ERROR;
            }
            options->seed_provided = true;
            seed_seen = true;
        } else {
            error_set(error, EXIT_CODE_USAGE, "unknown option '%s'", argv[index]);
            return CLI_PARSE_ERROR;
        }
    }

    return CLI_PARSE_OK;
}

void cli_print_usage(FILE *stream, const char *program_name)
{
    fprintf(stream,
            "Usage: %s <config-file> [--algorithm <name>] [--seed <uint64>]\n"
            "Algorithms: all, fcfs, sjf, srtf, priority-np, priority-p, rr, "
            "priority-rr\n",
            program_name);
}
