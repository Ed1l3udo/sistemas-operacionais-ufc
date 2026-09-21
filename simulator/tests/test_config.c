#include "test_common.h"

#include "config.h"

static bool parse_config(const char *text, Config *config, SchedulerError *error)
{
    FILE *stream = test_stream_from_text(text);
    bool parsed;

    TEST_ASSERT(stream != NULL);
    if (stream == NULL) {
        return false;
    }
    parsed = config_parse_stream(stream, config, error);
    TEST_ASSERT(fclose(stream) == 0);
    return parsed;
}

static void test_valid_configurations(void)
{
    Config config;
    SchedulerError error;

    error_clear(&error);
    TEST_ASSERT(parse_config("quantum:2\naging:0\n", &config, &error));
    TEST_ASSERT(config.quantum == 2 && config.aging == 0);
    error_clear(&error);
    TEST_ASSERT(parse_config("  aging : 1 \n\n quantum \t: \t3\n", &config,
                             &error));
    TEST_ASSERT(config.quantum == 3 && config.aging == 1);
}

static void test_invalid_configurations(void)
{
    static const char *cases[] = {
        "quantum:0\naging:1\n", "quantum:-1\naging:1\n",
        "quantum:1\naging:-1\n", "quantum:1\n",
        "quantum:1\nquantum:2\naging:0\n",
        "quantum:1\nother:0\naging:0\n",
        "quantum:word\naging:0\n",
        "quantum:999999999999999999999\naging:0\n",
        "quantum:1 trailing\naging:0\n"
    };
    size_t index;

    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        Config config;
        SchedulerError error;

        error_clear(&error);
        TEST_ASSERT(!parse_config(cases[index], &config, &error));
        TEST_ASSERT(error.code == EXIT_CODE_CONFIG);
        TEST_ASSERT(error.message[0] != '\0');
    }
}

static void test_missing_file(void)
{
    Config config;
    SchedulerError error;

    error_clear(&error);
    TEST_ASSERT(!config_load_file("missing-config-for-test.txt", &config, &error));
    TEST_ASSERT(error.code == EXIT_CODE_CONFIG);
}

static void test_valid_file(void)
{
    Config config;
    SchedulerError error;

    error_clear(&error);
    TEST_ASSERT(config_load_file("tests/fixtures/valid-config.txt", &config,
                                 &error));
    TEST_ASSERT(config.quantum == 2 && config.aging == 1);
}

void run_config_tests(void)
{
    test_valid_configurations();
    test_invalid_configurations();
    test_valid_file();
    test_missing_file();
}
