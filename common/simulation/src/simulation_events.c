#include "simulation_events.h"

#include <stdio.h>
#include <stdarg.h>

static void simulation_log_with_level(const char* level, const char* format, va_list args)
{
    printf("[SIM][%s] ", level);
    vprintf(format, args);
    printf("\n");
    fflush(stdout);
}

void simulation_log_info(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    simulation_log_with_level("INFO", format, args);
    va_end(args);
}

void simulation_log_warn(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    simulation_log_with_level("WARN", format, args);
    va_end(args);
}

void simulation_log_error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    simulation_log_with_level("ERROR", format, args);
    va_end(args);
}

void simulation_metric_cpu(uint32_t cpu_value)
{
    printf("[SIM][METRIC] cpu=%u\n", cpu_value);
    fflush(stdout);
}