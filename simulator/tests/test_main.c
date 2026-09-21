#include <stdio.h>

#include "test_common.h"

void run_cli_tests(void);
void run_config_tests(void);
void run_process_tests(void);
void run_integration_tests(void);

int main(void)
{
    run_process_tests();
    run_config_tests();
    run_cli_tests();
    run_integration_tests();

    if (test_failures != 0) {
        fprintf(stderr, "%d test assertion(s) failed.\n", test_failures);
        return 1;
    }

    puts("All stage 2 tests passed.");
    return 0;
}
