#include "test_common.h"

#include <stdlib.h>
#include <string.h>

#include "metrics.h"
#include "output.h"
#include "scheduler.h"

static char *read_stream(FILE *stream)
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

static bool line_has_no_trailing_spaces(const char *text)
{
    const char *cursor = text;

    while (*cursor != '\0') {
        const char *line_end = strchr(cursor, '\n');

        if (line_end == NULL) {
            line_end = cursor + strlen(cursor);
        }
        if (line_end > cursor && line_end[-1] == ' ') {
            return false;
        }
        if (*line_end == '\0') {
            break;
        }
        cursor = line_end + 1;
    }
    return true;
}

static void test_complete_short_output(void)
{
    const int values[][3] = {{1, 1, 1}, {0, 2, 1}, {3, 1, 1}};
    ProcessList source;
    Simulation simulation = {0};
    MetricsReport report;
    Config config = {2, 1};
    SchedulerError error;
    FILE *stream;
    char *text;

    process_list_init(&source);
    metrics_report_init(&report);
    error_clear(&error);
    TEST_ASSERT(process_list_append(&source, values[0][0], values[0][1],
                                    values[0][2], &error));
    TEST_ASSERT(process_list_append(&source, values[1][0], values[1][1],
                                    values[1][2], &error));
    TEST_ASSERT(process_list_append(&source, values[2][0], values[2][1],
                                    values[2][2], &error));
    TEST_ASSERT(scheduler_run_fcfs(&source, 0, &simulation, &error));
    TEST_ASSERT(metrics_calculate(&simulation, &report, &error));
    stream = tmpfile();
    TEST_ASSERT(stream != NULL);
    if (stream != NULL) {
        TEST_ASSERT(output_print_report(stream, ALGORITHM_FCFS, &config,
                                        &simulation, &report, &error));
        text = read_stream(stream);
        TEST_ASSERT(text != NULL);
        if (text != NULL) {
            TEST_ASSERT(strstr(text, "=== FCFS ===") != NULL);
            TEST_ASSERT(strstr(text, "Tempo médio de vida (turnaround, tt): 1.67") != NULL);
            TEST_ASSERT(strstr(text, "Tempo médio de espera (waiting, tw): 0.33") != NULL);
            TEST_ASSERT(strstr(text, "Tempo médio de resposta: 0.33") != NULL);
            TEST_ASSERT(strstr(text, "Trocas de contexto: 2") != NULL);
            TEST_ASSERT(strstr(text, "tempo") != NULL && strstr(text, "CPU") != NULL);
            TEST_ASSERT(strstr(text, "P1") != NULL && strstr(text, "P2") != NULL &&
                        strstr(text, "P3") != NULL);
            TEST_ASSERT(strstr(text, "0-1") != NULL && strstr(text, "2-3") != NULL);
            TEST_ASSERT(strstr(text, "##") != NULL && strstr(text, "--") != NULL);
            TEST_ASSERT(strstr(text, "..") != NULL && strstr(text, "OK") != NULL);
            TEST_ASSERT(strstr(text, "Legenda:") != NULL);
            TEST_ASSERT(line_has_no_trailing_spaces(text));
            free(text);
        }
        TEST_ASSERT(fclose(stream) == 0);
    }
    metrics_report_destroy(&report);
    simulation_destroy(&simulation);
    process_list_destroy(&source);
}

static void test_idle_and_round_robin_parameters(void)
{
    ProcessList source;
    Simulation simulation = {0};
    MetricsReport report;
    Config config = {1, 1};
    SchedulerError error;
    FILE *stream;
    char *text;

    process_list_init(&source);
    metrics_report_init(&report);
    error_clear(&error);
    TEST_ASSERT(process_list_append(&source, 2, 1, 1, &error));
    TEST_ASSERT(scheduler_run_round_robin(&source, 1, &simulation, &error));
    TEST_ASSERT(metrics_calculate(&simulation, &report, &error));
    stream = tmpfile();
    TEST_ASSERT(stream != NULL);
    if (stream != NULL) {
        TEST_ASSERT(output_print_report(stream, ALGORITHM_RR, &config, &simulation,
                                        &report, &error));
        text = read_stream(stream);
        TEST_ASSERT(text != NULL);
        if (text != NULL) {
            TEST_ASSERT(strstr(text, "=== Round-Robin ===") != NULL);
            TEST_ASSERT(strstr(text, "Quantum: 1") != NULL);
            TEST_ASSERT(strstr(text, "0-1") != NULL && strstr(text, "IDLE") != NULL);
            TEST_ASSERT(strstr(text, "IDLE = CPU ociosa") != NULL);
            free(text);
        }
        TEST_ASSERT(fclose(stream) == 0);
    }
    metrics_report_destroy(&report);
    simulation_destroy(&simulation);
    process_list_destroy(&source);
}

void run_output_tests(void)
{
    test_complete_short_output();
    test_idle_and_round_robin_parameters();
}
