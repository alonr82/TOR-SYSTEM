#ifndef SIMULATION_EVENTS_H
#define SIMULATION_EVENTS_H

#include <stdint.h>

void simulation_log_info(const char* format, ...);

void simulation_log_warn(const char* format, ...);

void simulation_log_error(const char* format, ...);

void simulation_metric_cpu(uint32_t cpu_value);

#endif