#ifndef SCHEDULER_METRICS_H
#define SCHEDULER_METRICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "simulation.h"

typedef struct {
    int process_id;
    int64_t turnaround_time;
    int64_t waiting_time;
    int64_t response_time;
} ProcessMetrics;

typedef struct {
    ProcessMetrics *processes;
    size_t process_count;
    double average_turnaround;
    double average_waiting;
    double average_response;
    size_t context_switches;
} MetricsReport;

void metrics_report_init(MetricsReport *report);
void metrics_report_destroy(MetricsReport *report);
bool metrics_calculate(const Simulation *simulation, MetricsReport *report,
                       SchedulerError *error);

#endif
