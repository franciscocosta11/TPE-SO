// tests/writer_tick.c
#define _POSIX_C_SOURCE 200809L   // POSIX
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>     // close, usleep
#include <fcntl.h>      // O_RDONLY, O_CREAT, O_RDWR, ...
#include <sys/mman.h>   // mmap, PROT_READ, PROT_WRITE, MAP_FAILED, ...
#include <sys/stat.h>   // struct stat, fstat
#include <sys/types.h>  // pid_t, off_t
#include <time.h>       // time
#include <errno.h>
#include <string.h>

#include "./../include/state.h"
#include "./../include/sync.h"
#include "./../include/state_access.h"

static void msleep(int ms) {
    struct timespec ts = { ms/1000, (ms%1000)*1000000L };
    nanosleep(&ts, NULL);
}

int main(void) {
    // Mapeo RW porque vamos a ESCRIBIR
    int gfd = shm_open(SHM_GAME_STATE, O_RDWR, 0600);
    if (gfd < 0) { perror("writer shm_open"); return 1; }

    struct stat st;
    if (fstat(gfd, &st) < 0) { perror("writer fstat"); close(gfd); return 1; }

    GameState *G = mmap(NULL, st.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, gfd, 0);
    if (G == MAP_FAILED) { perror("writer mmap"); close(gfd); return 1; }
    close(gfd);

    if (sync_attach() != 0) {
        fprintf(stderr,"[writer] sync_attach fail\n");
        munmap(G, st.st_size);
        return 1;
    }

    srand((unsigned)(time(NULL) ^ getpid()));

    const int DURATION_MS = 10 * 1000; // ~10 segundos
    int elapsed = 0;

    while (elapsed < DURATION_MS) {
        state_write_begin(); // = wrlock()
        if (G->game_over) {  // si el master ya terminó, salimos prolijo
            state_write_end();
            break;
        }

        if (G->w && G->h) {
            unsigned x = (unsigned)(rand() % G->w);
            unsigned y = (unsigned)(rand() % G->h);
            int *cell = &G->board[idx(G, x, y)];
            if (*cell > 1) (*cell) -= 1;   // pequeño “desgaste” si hay recompensa
        }

        state_write_end();   // = wrunlock()
        msleep(80);          // ~80 ms entre toques
        elapsed += 80;
    }

    munmap(G, st.st_size);
    return 0;
}
