#ifndef LOG_H
#define LOG_H

#include <stdarg.h>

/**
 * Log de información para stdout.
 * Uso: log_info("Iniciando vista. w=%d h=%d\n", w, h);
 */
void log_info(const char *fmt, ...);

/**
 * Log de error para stderr.
 * Uso: log_err("No pude abrir shm: %s\n", strerror(errno));
 */
void log_err(const char *fmt, ...);

#endif // LOG_H
