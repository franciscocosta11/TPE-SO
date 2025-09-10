#ifndef STATE_ACCESS_H
#define STATE_ACCESS_H

#include <stdbool.h>
#include "state.h"
#include "sync.h"

/* ---------- Sección de sincronización (wrappers simples) ---------- */

/**
 * @brief Inicia una sección de lectura protegida (múltiples lectores permitidos).
 *
 * Llamar antes de leer el GameState compartido. Internamente usa rdlock().
 */
static inline void state_read_begin(void)  { rdlock(); }

/**
 * @brief Finaliza una sección de lectura protegida.
 *
 * Llamar después de terminar de leer el GameState compartido. Internamente usa rdunlock().
 */
static inline void state_read_end(void)    { rdunlock(); }

/**
 * @brief Inicia una sección de escritura exclusiva sobre el GameState.
 *
 * Llamar antes de modificar el GameState compartido. Internamente usa wrlock().
 */
static inline void state_write_begin(void) { wrlock(); }

/**
 * @brief Finaliza una sección de escritura exclusiva.
 *
 * Llamar después de modificar el GameState compartido. Internamente usa wrunlock().
 */
static inline void state_write_end(void)   { wrunlock(); }

/* ---------- Helpers de lectura pública ---------- */

/**
 * @brief Devuelve el ancho del tablero contenido en G.
 * @param G Puntero al GameState (debe estar mapeado y protegido por read lock).
 * @return ancho (w) del tablero.
 */
unsigned state_get_w(const GameState *G);

/**
 * @brief Devuelve el alto del tablero contenido en G.
 * @param G Puntero al GameState (debe estar mapeado y protegido por read lock).
 * @return alto (h) del tablero.
 */
unsigned state_get_h(const GameState *G);

/**
 * @brief Devuelve el número de players activos en G.
 * @param G Puntero al GameState (debe estar mapeado y protegido por read lock).
 * @return cantidad de players (n_players).
 */
unsigned state_get_n(const GameState *G);

/**
 * @brief Indica si la partida terminó (flag game_over en el estado).
 * @param G Puntero al GameState (debe estar mapeado y protegido por read lock).
 * @return true si game_over está activado, false en caso contrario.
 */
bool state_is_over(const GameState *G);

/**
 * @brief Cuenta las recompensas remanentes (valores positivos) en el tablero.
 * @param G Puntero al GameState (debe estar mapeado y protegido por read lock).
 * @return número de celdas con recompensa > 0.
 */
int state_remaining_rewards(const GameState *G);

#endif
