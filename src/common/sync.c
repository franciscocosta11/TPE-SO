#define _POSIX_C_SOURCE 200809L
#include "sync.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "shm.h"

static SyncMem *S = NULL;
// static int sync_fd = -1;

// static int map_sync(int oflags, mode_t mode, int create) {
//     sync_fd = shm_open(SHM_GAME_SYNC, oflags, mode);
//     if (sync_fd == -1) { perror("shm_open(/game_sync)"); return -1; }
//     if (create) {
//         if (ftruncate(sync_fd, (off_t)sizeof(SyncMem)) == -1) { perror("ftruncate"); return -1; }
//     }
//     void *p = mmap(NULL, sizeof(SyncMem), PROT_READ|PROT_WRITE, MAP_SHARED, sync_fd, 0);
//     if (p == MAP_FAILED) { perror("mmap(/game_sync)"); return -1; }
//     S = (SyncMem*)p;
//     return 0;
// }

int sync_create(void) {
    S = shm_create_map(SHM_GAME_SYNC, sizeof(SyncMem), PROT_READ | PROT_WRITE);
    if (S == NULL) return -1;

    // Inicializar todos los semáforos y contadores
    if (sem_init(&S->view_update_ready, 1, 0) == -1) { perror("sem_init view_update_ready"); return -1; }
    if (sem_init(&S->view_render_complete, 1, 0) == -1) { perror("sem_init view_render_complete"); return -1; }
    if (sem_init(&S->writer_mutex, 1, 1) == -1) { perror("sem_init writer_mutex"); return -1; }
    if (sem_init(&S->state_mutex, 1, 1) == -1) { perror("sem_init state_mutex"); return -1; }
    if (sem_init(&S->readers_count_mutex, 1, 1) == -1) { perror("sem_init readers_count_mutex"); return -1; }
    
    S->readers_count = 0;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (sem_init(&S->player_turns[i], 1, 0) == -1) { 
            perror("sem_init player_turns"); 
            return -1; 
        }
    }

    return 0;
}

int sync_attach(void) {
    S = shm_attach_map(SHM_GAME_SYNC, NULL, PROT_READ | PROT_WRITE);
    return (S != NULL) ? 0 : -1;
}

void sync_destroy(void) {
    if (!S) return;

    sem_destroy(&S->view_update_ready);
    sem_destroy(&S->view_render_complete);
    sem_destroy(&S->writer_mutex);
    sem_destroy(&S->state_mutex);
    sem_destroy(&S->readers_count_mutex);

    for (int i = 0; i < MAX_PLAYERS; i++) {
        sem_destroy(&S->player_turns[i]);
    }

    munmap(S, sizeof(SyncMem));
    shm_remove_name(SHM_GAME_SYNC);
    S = NULL;
}

/* Lector-Escritor con prioridad para el escritor */

void rdlock(void) {
    // Un escritor puede estar esperando, así que pasamos por el torniquete.
    sem_wait(&S->writer_mutex);
    sem_post(&S->writer_mutex);

    // Bloqueamos para actualizar el contador de lectores de forma segura.
    sem_wait(&S->readers_count_mutex);
    S->readers_count++;
    if (S->readers_count == 1) {
        // Si somos el primer lector, bloqueamos el acceso a los escritores.
        sem_wait(&S->state_mutex);
    }
    sem_post(&S->readers_count_mutex);
}

void rdunlock(void) {
    // Bloqueamos para actualizar el contador de lectores de forma segura.
    sem_wait(&S->readers_count_mutex);
    S->readers_count--;
    if (S->readers_count == 0) {
        // Si somos el último lector, permitimos que los escritores pasen.
        sem_post(&S->state_mutex);
    }
    sem_post(&S->readers_count_mutex);
}

void wrlock(void) {
    // Bloqueamos el torniquete para que no entren nuevos lectores.
    sem_wait(&S->writer_mutex);
    // Esperamos a que los lectores actuales terminen y bloqueamos para escritura exclusiva.
    sem_wait(&S->state_mutex);
}

void wrunlock(void) {
    // Liberamos el bloqueo de escritura.
    sem_post(&S->state_mutex);
    // Abrimos el torniquete para que los lectores puedan volver a entrar.
    sem_post(&S->writer_mutex);
}
