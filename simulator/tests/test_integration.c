#include "test_common.h"

#include "config.h"
#include "process.h"

void run_integration_tests(void)
{
    Config config;
    ProcessList original;
    ProcessList copy;
    SchedulerError error;
    FILE *config_stream;
    FILE *process_stream;

    config_stream = test_stream_from_text("quantum:2\naging:1\n");
    process_stream = test_stream_from_text("0 2 1\n2 1 3\n");
    TEST_ASSERT(config_stream != NULL && process_stream != NULL);
    if (config_stream == NULL || process_stream == NULL) {
        if (config_stream != NULL) {
            (void)fclose(config_stream);
        }
        if (process_stream != NULL) {
            (void)fclose(process_stream);
        }
        return;
    }

    process_list_init(&original);
    process_list_init(&copy);
    error_clear(&error);
    TEST_ASSERT(config_parse_stream(config_stream, &config, &error));
    TEST_ASSERT(process_list_read(process_stream, &original, &error));
    TEST_ASSERT(process_list_clone(&original, &copy, &error));
    TEST_ASSERT(config.quantum == 2 && config.aging == 1);
    TEST_ASSERT(copy.count == 2 && copy.items[1].id == 2);
    process_list_destroy(&copy);
    process_list_destroy(&original);
    TEST_ASSERT(fclose(config_stream) == 0);
    TEST_ASSERT(fclose(process_stream) == 0);
}
