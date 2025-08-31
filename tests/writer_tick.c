#define _POSIX_C_SOURCE 200809L   // activa POSIX completo en glibc
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>     // close, ftruncate, etc.
#include <fcntl.h>      // O_RDONLY, O_CREAT, O_RDWR, ...
#include <sys/mman.h>   // mmap, PROT_READ, PROT_WRITE, MAP_FAILED, ...
#include <sys/stat.h>   // struct stat, fstat
#include <sys/types.h>  // pid_t, off_t
#include <errno.h>      // errno
#include <string.h>     // strerror
#include "./../include/state.h"
#include "./../include/sync.h"
#include "./../include/state_access.h"

int main(void) {
    int gfd = shm_open(SHM_GAME_STATE, O_RDWR, 0600);
    if (gfd < 0) { perror("writer shm_open"); return 1; }
    struct stat st; if (fstat(gfd, &st) < 0) { perror("writer fstat"); return 1; }
    GameState *G = mmap(NULL, st.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, gfd, 0);
    if (G == MAP_FAILED) { perror("writer mmap"); return 1; }
    close(gfd);
    if (sync_attach() != 0) { fprintf(stderr,"writer sync_attach fail\n"); return 1; }

    srand((unsigned)(time(NULL) ^ getpid()));
    for (int t=0; t<200; ++t) {
        state_write_begin();
        if (G->w && G->h) {
            unsigned x = (unsigned)(rand() % G->w);
            unsigned y = (unsigned)(rand() % G->h);
            int *cell = &G->board[idx(G,x,y)];
            if (*cell > 0) (*cell)--;          // pequeño cambio
        }
        state_write_end();
        sleep(5); // 50 ms
    }
    munmap(G, st.st_size);
    return 0;
}
