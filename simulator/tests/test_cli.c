#include "test_common.h"

#include "cli.h"

static CliParseResult parse_arguments(int argc, char *argv[], CliOptions *options,
                                      SchedulerError *error)
{
    error_clear(error);
    return cli_parse(argc, argv, options, error);
}

static void test_default_and_algorithms(void)
{
    static const char *names[] = {
        "all", "fcfs", "sjf", "srtf", "priority-np", "priority-p", "rr",
        "priority-rr"
    };
    static const AlgorithmType expected[] = {
        ALGORITHM_ALL, ALGORITHM_FCFS, ALGORITHM_SJF, ALGORITHM_SRTF,
        ALGORITHM_PRIORITY_NON_PREEMPTIVE, ALGORITHM_PRIORITY_PREEMPTIVE,
        ALGORITHM_RR, ALGORITHM_PRIORITY_RR
    };
    size_t index;
    CliOptions options;
    SchedulerError error;
    char *default_arguments[] = {"scheduler", "config.txt"};

    TEST_ASSERT(parse_arguments(2, default_arguments, &options, &error) ==
                CLI_PARSE_OK);
    TEST_ASSERT(options.algorithm == ALGORITHM_ALL);
    for (index = 0; index < sizeof(names) / sizeof(names[0]); ++index) {
        char *arguments[] = {"scheduler", "config.txt", "--algorithm",
                             (char *)names[index]};

        TEST_ASSERT(parse_arguments(4, arguments, &options, &error) ==
                    CLI_PARSE_OK);
        TEST_ASSERT(options.algorithm == expected[index]);
    }
}

static void test_seed_and_help(void)
{
    CliOptions options;
    SchedulerError error;
    char *zero_seed[] = {"scheduler", "config.txt", "--seed", "0"};
    char *large_seed[] = {"scheduler", "config.txt", "--seed",
                          "18446744073709551615"};
    char *help[] = {"scheduler", "--help"};

    TEST_ASSERT(parse_arguments(4, zero_seed, &options, &error) == CLI_PARSE_OK);
    TEST_ASSERT(options.seed_provided && options.seed == 0);
    TEST_ASSERT(parse_arguments(4, large_seed, &options, &error) == CLI_PARSE_OK);
    TEST_ASSERT(options.seed == UINT64_MAX);
    TEST_ASSERT(parse_arguments(2, help, &options, &error) == CLI_PARSE_HELP);
}

static void test_invalid_cli(void)
{
    static char *invalid_arguments[][5] = {
        {"scheduler", "config.txt", "--algorithm", "invalid", NULL},
        {"scheduler", "config.txt", "--seed", "-1", NULL},
        {"scheduler", "config.txt", "--seed", "word", NULL},
        {"scheduler", "config.txt", "--seed", "18446744073709551616", NULL},
        {"scheduler", "config.txt", "--unknown", NULL, NULL},
        {"scheduler", "config.txt", "--algorithm", NULL, NULL},
        {"scheduler", NULL, NULL, NULL, NULL}
    };
    static const int argument_counts[] = {4, 4, 4, 4, 3, 3, 1};
    size_t index;

    for (index = 0; index < sizeof(argument_counts) / sizeof(argument_counts[0]);
         ++index) {
        CliOptions options;
        SchedulerError error;

        TEST_ASSERT(parse_arguments(argument_counts[index],
                                    invalid_arguments[index], &options,
                                    &error) == CLI_PARSE_ERROR);
        TEST_ASSERT(error.code == EXIT_CODE_USAGE);
    }
}

void run_cli_tests(void)
{
    test_default_and_algorithms();
    test_seed_and_help();
    test_invalid_cli();
}
