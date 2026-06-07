/**
 * @file utils.c
 * @brief Utility functions implementation
 */

#include "utils.h"
#include <stdarg.h>
#include <sys/stat.h>
#include <ctype.h>

static LogLevel g_log_level = LOG_INFO;

void log_set_level(LogLevel level) {
    g_log_level = level;
}

static void log_print(LogLevel level, const char* prefix, const char* fmt, va_list args) {
    if (level < g_log_level) return;
    fprintf(stderr, "[%s] ", prefix);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
}

void log_debug(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_print(LOG_DEBUG, "DEBUG", fmt, args);
    va_end(args);
}

void log_info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_print(LOG_INFO, "INFO", fmt, args);
    va_end(args);
}

void log_warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_print(LOG_WARN, "WARN", fmt, args);
    va_end(args);
}

void log_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_print(LOG_ERROR, "ERROR", fmt, args);
    va_end(args);
}

void timer_start(Timer* t) {
    t->start = clock();
    t->end = 0;
    t->elapsed_ms = 0.0;
}

void timer_stop(Timer* t) {
    t->end = clock();
    t->elapsed_ms = ((double)(t->end - t->start)) * 1000.0 / CLOCKS_PER_SEC;
}

double timer_elapsed_ms(const Timer* t) {
    return t->elapsed_ms;
}

double timer_elapsed_s(const Timer* t) {
    return t->elapsed_ms / 1000.0;
}

double math_lcm(double a, double b) {
    if (a < 1.0 || b < 1.0) return 0.0;
    int ia = (int)a;
    int ib = (int)b;
    int x = ia, y = ib;
    while (y != 0) {
        int tmp = y;
        y = x % y;
        x = tmp;
    }
    int gcd = x;
    return (double)(ia * ib / gcd);
}

double math_gcd(double a, double b) {
    int ia = (int)fabs(a);
    int ib = (int)fabs(b);
    while (ib != 0) {
        int tmp = ib;
        ib = ia % ib;
        ia = tmp;
    }
    return (double)ia;
}

double math_round4(double x) {
    return round(x * 10000.0) / 10000.0;
}

bool file_exists(const char* path) {
    struct stat st;
    return (stat(path, &st) == 0);
}

bool file_write_string(const char* path, const char* content) {
    FILE* fp = fopen(path, "w");
    if (!fp) return false;
    fprintf(fp, "%s", content);
    fclose(fp);
    return true;
}

char* file_read_string(const char* path) {
    FILE* fp = fopen(path, "r");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char* buf = (char*)malloc(size + 1);
    if (!buf) {
        fclose(fp);
        return NULL;
    }
    fread(buf, 1, size, fp);
    buf[size] = '\0';
    fclose(fp);
    return buf;
}

char* str_trim(char* str) {
    if (!str) return NULL;
    while (isspace((unsigned char)*str)) str++;
    if (*str == '\0') return str;
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

char* str_duplicate(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char* dup = (char*)malloc(len);
    if (dup) memcpy(dup, str, len);
    return dup;
}
