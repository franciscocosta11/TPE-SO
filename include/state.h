#ifndef STATE_H
#define STATE_H

#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "shm.h"

#define MAX_PLAYERS 9
#define NAME_LEN    16
#define SHM_GAME_STATE "/game_state"

/**
 * @brief Direcciones de movimiento posibles.
 */
typedef enum {
    DIR_N  = 0,
    DIR_NE = 1,
    DIR_E  = 2,
    DIR_SE = 3,
    DIR_S  = 4,
    DIR_SW = 5,
    DIR_W  = 6,
    DIR_NW = 7
} Dir;

/**
 * @brief Información por jugador almacenada en el estado compartido.
 */
typedef struct Player {
    char name[NAME_LEN];    /**< @brief Nombre del jugador (string corto, sin \0 garantizado si overflow) */
    unsigned score;        /**< @brief Puntos acumulados */
    unsigned invalids;     /**< @brief Movimientos inválidos realizados */
    unsigned valids;       /**< @brief Movimientos válidos realizados */
    unsigned timeouts;     /**< @brief Turnos vencidos por timeout */
    unsigned short x, y;   /**< @brief Posición actual del jugador en el tablero */
    pid_t pid;             /**< @brief PID del proceso jugador (0 si no asignado) */
    bool blocked;          /**< @brief Marca si el jugador está bloqueado (sin movimientos legales) */
} Player;

/**
 * @brief Representación del estado completo del juego en memoria compartida.
 */
typedef struct GameState {
    unsigned short w, h;      /**< @brief ancho y alto del tablero */
    unsigned n_players;       /**< @brief número de players válidos en P[] */
    Player P[MAX_PLAYERS];    /**< @brief array de jugadores */
    bool game_over;           /**< @brief flag de fin de partida */
    int board[];              /**< @brief tablero (arreglo flexible) */
} GameState;

/**
 * @brief Crea y mapea un nuevo GameState en memoria compartida.
 * @param w ancho del tablero.
 * @param h alto del tablero.
 * @return puntero al GameState mapeado o NULL en error.
 */
GameState* state_create(unsigned w, unsigned h);

/**
 * @brief Se conecta a un GameState existente en memoria compartida.
 * @return puntero al GameState mapeado o NULL en error.
 */
GameState* state_attach(void);

/**
 * @brief Desmapea y libera recursos asociados a un GameState mapeado.
 * @param g puntero al GameState mapeado.
 */
void state_destroy(GameState *g);

/**
 * @brief Inicializa (pone a cero) un GameState recién creado.
 * @param g puntero al GameState.
 * @param w ancho.
 * @param h alto.
 * @param n número de players.
 */
void state_zero(GameState* g, unsigned w, unsigned h, unsigned n);

/**
 * @brief Calcula el tamaño en bytes necesario para un GameState con w×h.
 * @param w ancho.
 * @param h alto.
 * @return tamaño en bytes.
 */
size_t state_size(unsigned w, unsigned h);

/**
 * @brief Índice lineal de la celda (x,y) en el arreglo board.
 * @param g puntero al GameState.
 * @param x coordenada x.
 * @param y coordenada y.
 * @return índice en board (0..w*h-1).
 */
int idx(const GameState *g, unsigned x, unsigned y);

/**
 * @brief Rellena el tablero con recompensas usando una semilla.
 * @param g puntero al GameState (se modifica).
 * @param seed semilla para el generador.
 */
void board_fill_rewards(GameState *g, unsigned seed);

/**
 * @brief Coloca jugadores en una disposición inicial (ej. en cuadrícula).
 * @param g puntero al GameState (se modifica).
 */
void players_place_grid(GameState *g);

/* Helpers inline */

/**
 * @brief Obtiene la recompensa (>=0) según el valor almacenado en la celda.
 * @param v valor almacenado en board.
 * @return recompensa (0 si no hay).
 */
static inline int cell_reward(int v) { return v > 0 ? v : 0; }

/**
 * @brief Obtiene el índice del dueño de una celda capturada.
 * @param v valor almacenado en board.
 * @return índice del owner (0..n-1) o -1 si no está capturada.
 */
static inline int cell_owner(int v) { return v < 0 ? (-v - 1) : -1; }

/**
 * @brief Codifica una celda como capturada por owner_i.
 * @param owner_i índice del dueño.
 * @return valor a almacenar en board.
 */
static inline int make_captured(int owner_i) { return -(owner_i + 1); }


#endif // STATE_H
