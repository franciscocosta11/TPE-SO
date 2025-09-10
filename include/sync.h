#ifndef SYNC_H
#define SYNC_H
#include <semaphore.h>
#include <stddef.h>

#define SHM_GAME_SYNC "/game_sync"

#define MAX_PLAYERS 9

/**
 * @brief Crea e inicializa la memoria de sincronización (llamado por el master).
 * @return 0 en éxito, -1 en error (errno seteado).
 */
int sync_create(void);

/**
 * @brief Se conecta a la memoria de sincronización existente (view/players).
 * @return 0 en éxito, -1 en error (errno seteado).
 */
int sync_attach(void);

/**
 * @brief Destruye los recursos de sincronización (llamado por el master al finalizar).
 */
void sync_destroy(void);

/* --- Locks lectores/escritores sencillos --- */

/**
 * @brief Adquiere lock de lectura (permite múltiples lectores).
 */
void rdlock(void);

/**
 * @brief Libera lock de lectura.
 */
void rdunlock(void);

/**
 * @brief Adquiere lock de escritura (exclusivo).
 */
void wrlock(void);

/**
 * @brief Libera lock de escritura.
 */
void wrunlock(void);

/**
 * @brief Estructura almacenada en la memoria compartida de sincronización.
 *
 * Contiene semáforos usados por master/view/players para coordinar
 * actualizaciones y turnos.
 */
typedef struct SyncMem {
    sem_t view_update_ready;     /**< master -> view: señal para indicar estado listo */
    sem_t view_render_complete;  /**< view -> master : opcional, indica render finalizado */
    sem_t writer_mutex;          /**< mutex general para escrituras criticas */
    sem_t state_mutex;           /**< mutex usado para secciones críticas sobre el state */
    sem_t readers_count_mutex;   /**< mutex que protege readers_count */
    unsigned int readers_count;  /**< contador de lectores concurrentes */
    sem_t player_turns[MAX_PLAYERS]; /**< semáforos por jugador para habilitar turnos */
} SyncMem;

/* --- API master <-> view --- */

/**
 * @brief Señaliza a la view que hay un update listo (master -> view).
 */
void view_signal_update_ready(void);

/**
 * @brief Espera hasta que el master indique que hay un update listo (view <- master).
 */
void view_wait_update_ready(void);

/**
 * @brief Señaliza al master que la view terminó de renderizar (view -> master).
 *
 * Esta función es opcional: si la view no la usa no bloquea al master.
 */
void view_signal_render_complete(void);

/**
 * @brief Espera a que la view indique que terminó de renderizar (master <- view).
 */
void view_wait_render_complete(void);

/* --- API master <-> players (turnos) --- */

/**
 * @brief Habilita el turno del jugador i (master -> player i).
 * @param i índice del jugador (0..MAX_PLAYERS-1)
 */
void player_signal_turn(int i);

/**
 * @brief Espera bloqueantemente hasta que el master habilite el turno (player side).
 * @param i índice del jugador (0..MAX_PLAYERS-1)
 */
void player_wait_turn(int i);

/**
 * @brief Espera el turno con timeout.
 *
 * @param i índice del jugador (0..MAX_PLAYERS-1)
 * @param timeout_ms tiempo máximo en milisegundos a esperar (0 = bloquear indefinidamente)
 * @return  1 si el turno fue otorgado,
 *          0 si hubo timeout,
 *         -1 en caso de error (errno seteado).
 */
int player_wait_turn_timed(int i, int timeout_ms);



#endif
