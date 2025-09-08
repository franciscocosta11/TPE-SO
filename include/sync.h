#ifndef SYNC_H
#define SYNC_H
#include <semaphore.h>
#include <stddef.h>

#define SHM_GAME_SYNC "/game_sync"

#define MAX_PLAYERS 9

int sync_create(void);   // lo llama el master (crea/trunca shm y semáforos)
int sync_attach(void);   // lo llama la view/jugadores (solo adjunta)
void sync_destroy(void); // destruir (solo master al final)

void rdlock(void);
void rdunlock(void);
void wrlock(void);
void wrunlock(void);

typedef struct
{
    sem_t view_update_ready;
    sem_t view_render_complete;
    sem_t writer_mutex;
    sem_t state_mutex;
    sem_t readers_count_mutex;
    unsigned int readers_count;
    sem_t player_turns[MAX_PLAYERS];
} SyncMem;

/* Sincronización Master <-> View */
void view_signal_update_ready(void);    // master -> view
void view_wait_update_ready(void);      // view   <- master

void view_signal_render_complete(void); // view   -> master (opcional)
void view_wait_render_complete(void);   // master <- view   (opcional)

void player_signal_turn(int i);                        // master -> habilita turno de jugador i
void player_wait_turn(int i);                          // player i espera turno (bloqueante)
int  player_wait_turn_timed(int i, int timeout_ms);    // player i espera con timeout (1 ok, 0 timeout, -1 error)



#endif
