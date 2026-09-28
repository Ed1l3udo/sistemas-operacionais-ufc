#include "test_common.h"

#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "error.h"

static char *stream_text(FILE *stream)
{
    char *text;
    long size;

    if (fflush(stream) != 0 || fseek(stream, 0, SEEK_END) != 0 ||
        (size = ftell(stream)) < 0 || fseek(stream, 0, SEEK_SET) != 0) {
        return NULL;
    }
    text = malloc((size_t)size + 1);
    if (text == NULL || fread(text, 1, (size_t)size, stream) != (size_t)size) {
        free(text);
        return NULL;
    }
    text[size] = '\0';
    return text;
}

static int run_app(char *arguments[], int argc, const char *input_text,
                   FILE **output, FILE **errors)
{
    FILE *input = test_stream_from_text(input_text);
    int result;

    *output = tmpfile();
    *errors = tmpfile();
    if (input == NULL || *output == NULL || *errors == NULL) {
        if (input != NULL) {
            (void)fclose(input);
        }
        return EXIT_CODE_INTERNAL;
    }
    result = scheduler_run_application(argc, arguments, input, *output, *errors);
    (void)fclose(input);
    return result;
}

static void test_individual_algorithms_and_all(void)
{
    static const char *names[] = {
        "fcfs", "sjf", "srtf", "priority-np", "priority-p", "rr",
        "priority-rr"
    };
    static const char *titles[] = {
        "=== FCFS ===", "=== Shortest Job First (SJF) ===",
        "=== Shortest Remaining Time First (SRTF) ===",
        "=== Prioridade sem preempção ===", "=== Prioridade com preempção ===",
        "=== Round-Robin ===", "=== Round-Robin com prioridade e aging ==="
    };
    const char *input_text = "0 5 2\n0 2 3\n1 4 1\n3 3 4\n";
    size_t index;

    for (index = 0; index < sizeof(names) / sizeof(names[0]); ++index) {
        char *arguments[] = {"scheduler", "tests/fixtures/valid-config.txt",
                             "--algorithm", (char *)names[index], "--seed", "0"};
        FILE *output;
        FILE *errors;
        char *text;
        char *error_text;

        TEST_ASSERT(run_app(arguments, 6, input_text, &output, &errors) == 0);
        text = stream_text(output);
        error_text = stream_text(errors);
        TEST_ASSERT(text != NULL && error_text != NULL);
        if (text != NULL && error_text != NULL) {
            TEST_ASSERT(strstr(text, "Seed: 0") != NULL);
            TEST_ASSERT(strstr(text, titles[index]) != NULL);
            TEST_ASSERT(error_text[0] == '\0');
        }
        free(error_text);
        free(text);
        TEST_ASSERT(fclose(errors) == 0);
        TEST_ASSERT(fclose(output) == 0);
    }
    {
        char *arguments[] = {"scheduler", "tests/fixtures/valid-config.txt",
                             "--seed", "0"};
        FILE *output;
        FILE *errors;
        char *text;
        const char *previous = NULL;

        TEST_ASSERT(run_app(arguments, 4, input_text, &output, &errors) == 0);
        text = stream_text(output);
        TEST_ASSERT(text != NULL);
        if (text != NULL) {
            for (index = 0; index < sizeof(titles) / sizeof(titles[0]); ++index) {
                const char *current = strstr(text, titles[index]);

                TEST_ASSERT(current != NULL);
                TEST_ASSERT(previous == NULL || current > previous);
                previous = current;
            }
            free(text);
        }
        TEST_ASSERT(fclose(errors) == 0);
        TEST_ASSERT(fclose(output) == 0);
    }
}

static void test_seed_reproducibility_and_errors(void)
{
    const char *input_text = "0 2 1\n0 1 2\n";
    char *first_arguments[] = {"scheduler", "tests/fixtures/valid-config.txt",
                               "--algorithm", "srtf", "--seed", "42"};
    char *invalid_algorithm[] = {"scheduler", "tests/fixtures/valid-config.txt",
                                 "--algorithm", "bad"};
    char *bad_config[] = {"scheduler", "tests/fixtures/missing.txt"};
    char *automatic_seed[] = {"scheduler", "tests/fixtures/valid-config.txt",
                              "--algorithm", "fcfs"};
    char *valid_arguments[] = {"scheduler", "tests/fixtures/valid-config.txt",
                               "--algorithm", "fcfs", "--seed", "0"};
    char *help[] = {"scheduler", "--help"};
    FILE *first_output;
    FILE *first_errors;
    FILE *second_output;
    FILE *second_errors;
    FILE *output;
    FILE *errors;
    char *first_text;
    char *second_text;
    char *error_text;

    TEST_ASSERT(run_app(first_arguments, 6, input_text, &first_output,
                        &first_errors) == 0);
    TEST_ASSERT(run_app(first_arguments, 6, input_text, &second_output,
                        &second_errors) == 0);
    first_text = stream_text(first_output);
    second_text = stream_text(second_output);
    TEST_ASSERT(first_text != NULL && second_text != NULL);
    if (first_text != NULL && second_text != NULL) {
        TEST_ASSERT(strcmp(first_text, second_text) == 0);
    }
    free(second_text);
    free(first_text);
    TEST_ASSERT(fclose(second_errors) == 0 && fclose(second_output) == 0);
    TEST_ASSERT(fclose(first_errors) == 0 && fclose(first_output) == 0);

    TEST_ASSERT(run_app(invalid_algorithm, 4, input_text, &output, &errors) ==
                EXIT_CODE_USAGE);
    error_text = stream_text(errors);
    TEST_ASSERT(error_text != NULL && strstr(error_text, "unknown algorithm") != NULL);
    free(error_text);
    TEST_ASSERT(fclose(errors) == 0 && fclose(output) == 0);
    TEST_ASSERT(run_app(bad_config, 2, input_text, &output, &errors) ==
                EXIT_CODE_CONFIG);
    error_text = stream_text(errors);
    TEST_ASSERT(error_text != NULL && error_text[0] != '\0');
    free(error_text);
    TEST_ASSERT(fclose(errors) == 0 && fclose(output) == 0);
    TEST_ASSERT(run_app(valid_arguments, 6, "invalid\n", &output, &errors) ==
                EXIT_CODE_INPUT);
    first_text = stream_text(output);
    error_text = stream_text(errors);
    TEST_ASSERT(first_text != NULL && first_text[0] == '\0');
    TEST_ASSERT(error_text != NULL && error_text[0] != '\0');
    free(error_text);
    free(first_text);
    TEST_ASSERT(fclose(errors) == 0 && fclose(output) == 0);
    TEST_ASSERT(run_app(automatic_seed, 4, input_text, &output, &errors) == 0);
    first_text = stream_text(output);
    error_text = stream_text(errors);
    TEST_ASSERT(first_text != NULL && strstr(first_text, "Seed: ") != NULL);
    TEST_ASSERT(error_text != NULL && error_text[0] == '\0');
    free(error_text);
    free(first_text);
    TEST_ASSERT(fclose(errors) == 0 && fclose(output) == 0);
    TEST_ASSERT(run_app(help, 2, input_text, &output, &errors) == 0);
    error_text = stream_text(errors);
    TEST_ASSERT(error_text != NULL && error_text[0] == '\0');
    free(error_text);
    TEST_ASSERT(fclose(errors) == 0 && fclose(output) == 0);
}

void run_app_tests(void)
{
    test_individual_algorithms_and_all();
    test_seed_reproducibility_and_errors();
}
