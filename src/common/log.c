// src/common/log.c 
#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

// Devuelve timestamp "YYYY-MM-DD HH:MM:SS" en buf
static void ts_now(char *buf, size_t n) {
    time_t t = time(NULL);
    struct tm tm_buf;

    // Windows / MSVC / MinGW: localtime_s(dst, src)
    // POSIX: localtime_r(src, dst)
    // Fallback: localtime (no thread-safe)
#if defined(_WIN32)
    localtime_s(&tm_buf, &t);
#elif defined(_POSIX_VERSION)
    localtime_r(&t, &tm_buf);
#else
    struct tm *tmp = localtime(&t);
    if (tmp) tm_buf = *tmp;
#endif

    strftime(buf, n, "%Y-%m-%d %H:%M:%S", &tm_buf);
}

void log_info(const char *fmt, ...) {
    char ts[32];
    ts_now(ts, sizeof(ts));
    fprintf(stdout, "[INFO] %s | ", ts);
    va_list ap; va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    fflush(stdout);
}

void log_err(const char *fmt, ...) {
    char ts[32];
    ts_now(ts, sizeof(ts));
    fprintf(stderr, "[ERROR] %s | ", ts);
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fflush(stderr);
}
