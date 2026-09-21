#ifndef SCHEDULER_TEST_COMMON_H
#define SCHEDULER_TEST_COMMON_H

#include <stdbool.h>
#include <stdio.h>

extern int test_failures;

void test_assert(bool condition, const char *expression, const char *file,
                 int line);
FILE *test_stream_from_text(const char *text);

#define TEST_ASSERT(expression) \
    test_assert((expression), #expression, __FILE__, __LINE__)

#endif
