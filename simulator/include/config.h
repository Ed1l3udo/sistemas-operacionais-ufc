#ifndef SCHEDULER_CONFIG_H
#define SCHEDULER_CONFIG_H

#include <stdbool.h>
#include <stdio.h>

#include "error.h"

typedef struct {
    int quantum;
    int aging;
} Config;

bool config_parse_stream(FILE *input, Config *config, SchedulerError *error);
bool config_load_file(const char *path, Config *config, SchedulerError *error);

#endif
