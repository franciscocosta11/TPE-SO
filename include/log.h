#ifndef LOG_H
#define LOG_H

#include <stdarg.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

/** 
 * @brief Helper interno: imprime un mensaje con timestamp y flush.
 * @param out FILE* destino (stdout/stderr)
 * @param level Etiqueta del nivel ("INFO", "ERROR", ...)
 * @param fmt Formato printf-style
 * @param ap va_list con los argumentos ya inicializados
 */
void log_do(FILE *out, const char *level, const char *fmt, va_list ap);

/**
 * @brief Escribe un mensaje informativo en stdout.
 * @param fmt Cadena de formato (printf-style).
 * @param ... Argumentos para la cadena de formato.
 */
void log_info(const char *fmt, ...);

/**
 * @brief Escribe un mensaje de error en stderr.
 * @param fmt Cadena de formato (printf-style).
 * @param ... Argumentos para la cadena de formato.
 */
void log_err(const char *fmt, ...);

#endif // LOG_H
