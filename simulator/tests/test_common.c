#include "test_common.h"

#include <stdlib.h>

int test_failures = 0;

void test_assert(bool condition, const char *expression, const char *file,
                 int line)
{
    if (!condition) {
        ++test_failures;
        fprintf(stderr, "%s:%d: assertion failed: %s\n", file, line,
                expression);
    }
}

FILE *test_stream_from_text(const char *text)
{
    FILE *stream = tmpfile();

    if (stream == NULL) {
        return NULL;
    }
    if (fputs(text, stream) == EOF || fflush(stream) != 0) {
        (void)fclose(stream);
        return NULL;
    }
    rewind(stream);
    return stream;
}
