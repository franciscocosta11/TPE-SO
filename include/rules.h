#ifndef RULES_H
#define RULES_H

#include "state.h"

/**
 * @brief Valida si el jugador 'pid' puede moverse en la dirección 'd'.
 * @param g Puntero al estado del juego (lectura).
 * @param pid Índice del jugador (0..n_players-1).
 * @param d Dirección a validar (Dir).
 * @param[out] gain Si no es NULL, recibe la ganancia de la celda destino.
 * @return 1 si el movimiento es válido, 0 si no lo es.
 */
int rules_validate(const GameState *g, int pid, Dir d, int *gain);

/**
 * @brief Aplica el movimiento del jugador 'pid' en la dirección 'd'.
 * @param g Puntero al estado del juego (se modifica).
 * @param pid Índice del jugador.
 * @param d Dirección a aplicar.
 */
void rules_apply(GameState *g, int pid, Dir d);

/**
 * @brief Comprueba si el jugador 'pid' tiene al menos un movimiento legal.
 * @param g Puntero al estado del juego (lectura).
 * @param pid Índice del jugador.
 * @return 1 si tiene al menos un movimiento legal, 0 si está bloqueado.
 */
int player_can_move(const GameState *g, int pid);


#endif // RULES_H