#ifndef SCHEDULER_ERROR_H
#define SCHEDULER_ERROR_H

#include <stdarg.h>

/* Exit codes are shared by the CLI, parsers, and main program. */
typedef enum {
    EXIT_CODE_SUCCESS = 0,
    EXIT_CODE_USAGE = 2,
    EXIT_CODE_CONFIG = 3,
    EXIT_CODE_INPUT = 4,
    EXIT_CODE_INTERNAL = 5
} ExitCode;

#define ERROR_MESSAGE_CAPACITY 512

typedef struct {
    ExitCode code;
    char message[ERROR_MESSAGE_CAPACITY];
} SchedulerError;

void error_clear(SchedulerError *error);
void error_set(SchedulerError *error, ExitCode code, const char *format, ...);

#endif
