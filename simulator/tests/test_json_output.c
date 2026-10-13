#include "test_common.h"

#include <stdlib.h>
#include <string.h>

#include "json_output.h"
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

static bool make_result(Simulation *simulation, MetricsReport *report,
                        SchedulerError *error)
{
    ProcessList source;
    bool success;

    process_list_init(&source);
    metrics_report_init(report);
    if (!process_list_append(&source, 0, 2, 2, error) ||
        !process_list_append(&source, 0, 1, 1, error)) {
        process_list_destroy(&source);
        return false;
    }
    success = scheduler_run_fcfs(&source, 0, simulation, error) &&
              metrics_calculate(simulation, report, error);
    process_list_destroy(&source);
    return success;
}

static void test_document_and_escaping(void)
{
    Simulation simulation = {0};
    MetricsReport report;
    Config config = {2, 1};
    SchedulerError error;
    FILE *stream;
    char *text;

    error_clear(&error);
    TEST_ASSERT(make_result(&simulation, &report, &error));
    stream = tmpfile();
    TEST_ASSERT(stream != NULL);
    if (stream != NULL) {
        TEST_ASSERT(json_output_begin_document(stream, UINT64_MAX, &config, &error));
        TEST_ASSERT(json_output_write_algorithm(stream, ALGORITHM_FCFS, &simulation,
                                                 &report, false, &error));
        TEST_ASSERT(json_output_end_document(stream, &error));
        text = read_stream(stream);
        TEST_ASSERT(text != NULL);
        if (text != NULL) {
            TEST_ASSERT(strstr(text, "\"schemaVersion\": 1") != NULL);
            TEST_ASSERT(strstr(text, "\"seed\": \"18446744073709551615\"") != NULL);
            TEST_ASSERT(strstr(text, "\"quantum\": 2") != NULL);
            TEST_ASSERT(strstr(text, "\"id\": \"fcfs\"") != NULL);
            TEST_ASSERT(strstr(text, "\"name\": \"FCFS\"") != NULL);
            TEST_ASSERT(strstr(text, "\"averageTurnaround\": 2") != NULL);
            TEST_ASSERT(strstr(text, "\"finalDynamicPriority\": 2") != NULL);
            TEST_ASSERT(strstr(text, ",]") == NULL && strstr(text, ",}") == NULL);
            TEST_ASSERT(text[strlen(text) - 1] == '\n');
            free(text);
        }
        TEST_ASSERT(fclose(stream) == 0);
    }
    metrics_report_destroy(&report);
    simulation_destroy(&simulation);

    stream = tmpfile();
    TEST_ASSERT(stream != NULL);
    if (stream != NULL) {
        TEST_ASSERT(json_output_write_escaped_string(stream,
                                                      "\"\\\b\f\n\r\t\001", &error));
        text = read_stream(stream);
        TEST_ASSERT(text != NULL);
        if (text != NULL) {
            TEST_ASSERT(strcmp(text, "\"\\\"\\\\\\b\\f\\n\\r\\t\\u0001\"") == 0);
            free(text);
        }
        TEST_ASSERT(fclose(stream) == 0);
    }
}

static void test_idle_timeline_and_immutability(void)
{
    ProcessList source;
    Simulation simulation = {0};
    MetricsReport report;
    Config config = {1, 0};
    SchedulerError error;
    FILE *stream;
    char *text;
    int final_priority;
    int64_t completion_time;

    process_list_init(&source);
    metrics_report_init(&report);
    error_clear(&error);
    TEST_ASSERT(process_list_append(&source, 2, 1, 1, &error));
    TEST_ASSERT(scheduler_run_round_robin(&source, 1, &simulation, &error));
    TEST_ASSERT(metrics_calculate(&simulation, &report, &error));
    final_priority = simulation.processes.items[0].dynamic_priority;
    completion_time = simulation.processes.items[0].completion_time;
    stream = tmpfile();
    TEST_ASSERT(stream != NULL);
    if (stream != NULL) {
        TEST_ASSERT(json_output_begin_document(stream, 0, &config, &error));
        TEST_ASSERT(json_output_write_algorithm(stream, ALGORITHM_RR, &simulation,
                                                 &report, false, &error));
        TEST_ASSERT(json_output_end_document(stream, &error));
        text = read_stream(stream);
        TEST_ASSERT(text != NULL);
        if (text != NULL) {
            TEST_ASSERT(strstr(text, "\"processId\": null") != NULL);
            free(text);
        }
        TEST_ASSERT(simulation.processes.items[0].dynamic_priority == final_priority);
        TEST_ASSERT(simulation.processes.items[0].completion_time == completion_time);
        TEST_ASSERT(fclose(stream) == 0);
    }
    metrics_report_destroy(&report);
    simulation_destroy(&simulation);
    process_list_destroy(&source);
}

void run_json_output_tests(void)
{
    test_document_and_escaping();
    test_idle_timeline_and_immutability();
}
