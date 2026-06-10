/**
 * @file utils.h
 * @brief Utility functions interface
 */

#ifndef UTILS_H
#define UTILS_H

#include "rt_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LogLevel;

void log_set_level(LogLevel level);
void log_debug(const char* fmt, ...);
void log_info(const char* fmt, ...);
void log_warn(const char* fmt, ...);
void log_error(const char* fmt, ...);

typedef struct {
    clock_t start;
    clock_t end;
    double elapsed_ms;
} Timer;

void timer_start(Timer* t);
void timer_stop(Timer* t);
double timer_elapsed_ms(const Timer* t);
double timer_elapsed_s(const Timer* t);

double math_lcm(double a, double b);
double math_gcd(double a, double b);
double math_round4(double x);

bool file_exists(const char* path);
bool file_write_string(const char* path, const char* content);
char* file_read_string(const char* path);

char* str_trim(char* str);
char* str_duplicate(const char* str);

#ifdef __cplusplus
}
#endif

#endif
