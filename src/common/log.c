// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
// src/common/log.c
#include "log.h"

// Devuelve timestamp "YYYY-MM-DD HH:MM:SS" en buf
static void ts_now(char *buf, size_t n)
{
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
    if (tmp)
        tm_buf = *tmp;
#endif

    strftime(buf, n, "%Y-%m-%d %H:%M:%S", &tm_buf);
}

void log_do(FILE *out, const char *level, const char *fmt, va_list ap)
{
    char ts[32];
    ts_now(ts, sizeof(ts));
    fprintf(out, "[%s] %s | ", level, ts);
    vfprintf(out, fmt, ap);
    fflush(out);
}

void log_info(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_do(stdout, "INFO", fmt, ap);
    va_end(ap);
}

void log_err(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_do(stderr, "ERROR", fmt, ap);
    va_end(ap);
}
