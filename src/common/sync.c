#define _POSIX_C_SOURCE 200809L
#include "sync.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    sem_t C;   // torniquete (evita inanición escritor)
    sem_t D;   // exclusión de escritura
    sem_t E;   // protege contador F
    int   F;   // # lectores activos
} SyncMem;

static SyncMem *S = NULL;
static int sync_fd = -1;

static int map_sync(int oflags, mode_t mode, int create) {
    sync_fd = shm_open(SHM_GAME_SYNC, oflags, mode);
    if (sync_fd == -1) { perror("shm_open(/game_sync)"); return -1; }
    if (create) {
        if (ftruncate(sync_fd, (off_t)sizeof(SyncMem)) == -1) { perror("ftruncate"); return -1; }
    }
    void *p = mmap(NULL, sizeof(SyncMem), PROT_READ|PROT_WRITE, MAP_SHARED, sync_fd, 0);
    if (p == MAP_FAILED) { perror("mmap(/game_sync)"); return -1; }
    S = (SyncMem*)p;
    return 0;
}

int sync_create(void) {
    if (map_sync(O_CREAT|O_RDWR, 0600, 1) != 0) return -1;
    // inicializar semáforos en memoria compartida (pshared = 1)
    if (sem_init(&S->C, 1, 1) == -1) { perror("sem_init C"); return -1; }
    if (sem_init(&S->D, 1, 1) == -1) { perror("sem_init D"); return -1; }
    if (sem_init(&S->E, 1, 1) == -1) { perror("sem_init E"); return -1; }
    S->F = 0;
    return 0;
}

int sync_attach(void) {
    return map_sync(O_RDWR, 0600, 0);
}

void sync_destroy(void) {
    if (!S) return;
    sem_destroy(&S->C);
    sem_destroy(&S->D);
    sem_destroy(&S->E);
    munmap(S, sizeof(SyncMem));
    if (sync_fd != -1) close(sync_fd);
    shm_unlink(SHM_GAME_SYNC);
    S = NULL; sync_fd = -1;
}

/* Lectores-escritor sin inanición del escritor (torniquete C) */
void rdlock(void) {
    sem_wait(&S->C);      // pasar por torniquete
    sem_post(&S->C);
    sem_wait(&S->E);      // actualizar F
    S->F++;
    if (S->F == 1) sem_wait(&S->D);
    sem_post(&S->E);
}
void rdunlock(void) {
    sem_wait(&S->E);
    S->F--;
    if (S->F == 0) sem_post(&S->D);
    sem_post(&S->E);
}
void wrlock(void) {
    sem_wait(&S->C);      // bloquear nuevas lecturas
    sem_wait(&S->D);      // exclusión de escritura
}
void wrunlock(void) {
    sem_post(&S->D);
    sem_post(&S->C);
}
