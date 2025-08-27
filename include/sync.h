#ifndef SYNC_H
#define SYNC_H
#include <semaphore.h>
#include <stddef.h>

#define SHM_GAME_STATE "/game_state"
#define SHM_GAME_SYNC  "/game_sync"

int  sync_create(void);   // lo llama el master (crea/trunca shm y semáforos)
int  sync_attach(void);   // lo llama la view/jugadores (solo adjunta)
void sync_destroy(void);  // destruir (solo master al final)

void rdlock(void);
void rdunlock(void);
void wrlock(void);
void wrunlock(void);

#endif
