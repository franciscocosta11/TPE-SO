#ifndef STATE_H
#define STATE_H

#include <stddef.h>
#include <stdint.h>

/**
 * Estructura mínima del estado compartido que la vista necesita
 * para render básico. B puede extenderla, pero estos campos
 * deben preservarse para no romper compatibilidad binaria.
 *
 * Layout esperado:
 *  - w, h: dimensiones del tablero
 *  - n_players: cantidad de jugadores
 *  - game_over: bandera de fin de juego (0/1)
 *  - board: puntero a arreglo contiguo de w*h celdas (enteros)
 *
 * Convención de celdas (placeholder visual):
 *  - >0 : recompensa (1..9)
 *  -  0 : libre
 *  - <0 : capturada (valor negativo suele codificar dueño)
 */
typedef struct GameState {
    int32_t w;
    int32_t h;
    int32_t n_players;
    int32_t game_over;
    // Nota: el tablero suele estar contiguo a la estructura o referenciado.
    // Para la vista asumimos que board es un puntero válido en el mapeo.
    int32_t *board;

    // Sugerido (la vista puede usarlo si existe):
    struct {
        int32_t x, y;
        int32_t score;
        int32_t alive;
    } P[8]; // hasta 8 jugadores, extensible
} GameState;

/** índice lineal en el tablero contiguo */
static inline size_t idx(const GameState *gs, int x, int y) {
    return (size_t)y * (size_t)gs->w + (size_t)x;
}

#endif // STATE_H
