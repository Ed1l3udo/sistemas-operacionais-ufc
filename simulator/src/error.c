#include "error.h"

#include <stdio.h>

void error_clear(SchedulerError *error)
{
    if (error == NULL) {
        return;
    }

    error->code = EXIT_CODE_SUCCESS;
    error->message[0] = '\0';
}

void error_set(SchedulerError *error, ExitCode code, const char *format, ...)
{
    va_list arguments;

    if (error == NULL) {
        return;
    }

    error->code = code;
    va_start(arguments, format);
    (void)vsnprintf(error->message, sizeof(error->message), format, arguments);
    va_end(arguments);
}
