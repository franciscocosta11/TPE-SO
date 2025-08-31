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
    int gfd = shm_open(SHM_GAME_STATE, O_RDONLY, 0600);
    if (gfd < 0) { perror("reader shm_open"); return 1; }
    struct stat st; if (fstat(gfd, &st) < 0) { perror("reader fstat"); return 1; }
    const GameState *G = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, gfd, 0);
    if (G == MAP_FAILED) { perror("reader mmap"); return 1; }
    close(gfd);
    if (sync_attach() != 0) { fprintf(stderr,"reader sync_attach fail\n"); return 1; }

    for (int t=0; t<200; ++t) {
        state_read_begin();
        volatile unsigned w = G->w, h = G->h; (void)w; (void)h;
        // leer algunas celdas
        if (G->w && G->h) { volatile int v = G->board[idx(G, 0, 0)]; (void)v; }
        state_read_end();
        sleep(2); // 20 ms
    }
    munmap((void*)G, st.st_size);
    return 0;
}
