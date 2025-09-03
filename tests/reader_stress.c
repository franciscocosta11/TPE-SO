// tests/reader_stress.c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>     // close
#include <fcntl.h>      // O_RDONLY
#include <sys/mman.h>   // mmap
#include <sys/stat.h>   // fstat
#include <sys/types.h>
#include <time.h>

#include "./../include/state.h"
#include "./../include/sync.h"
#include "./../include/state_access.h"

static void msleep(int ms) {
    struct timespec ts = { ms/1000, (ms%1000)*1000000L };
    nanosleep(&ts, NULL);
}

int main(void) {
    // Mapeo sólo LECTURA
    int gfd = shm_open(SHM_GAME_STATE, O_RDONLY, 0600);
    if (gfd < 0) { perror("reader shm_open"); return 1; }

    struct stat st;
    if (fstat(gfd, &st) < 0) { perror("reader fstat"); close(gfd); return 1; }

    const GameState *G = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, gfd, 0);
    if (G == MAP_FAILED) { perror("reader mmap"); close(gfd); return 1; }
    close(gfd);

    if (sync_attach() != 0) {
        fprintf(stderr,"[reader] sync_attach fail\n");
        munmap((void*)G, st.st_size);
        return 1;
    }

    const int DURATION_MS = 10 * 1000; // ~10 segundos de lectura concurrente
    int elapsed = 0;

    while (elapsed < DURATION_MS) {
        state_read_begin();  // = rdlock()

        unsigned w = G->w, h = G->h;
        (void)w; (void)h;

        // Leer algunas celdas “de muestra” para ejercitar el lock
        int sample = 0;
        if (G->w && G->h) {
            unsigned stepY = (G->h/4) ? (G->h/4) : 1;
            unsigned stepX = (G->w/5) ? (G->w/5) : 1;
            for (unsigned y = 0; y < G->h; y += stepY) {
                for (unsigned x = 0; x < G->w; x += stepX) {
                    sample += G->board[idx(G, x, y)];
                }
            }
        }

        bool over = G->game_over;
        state_read_end();    // = rdunlock()

        if (over) break;     // si el juego terminó, salimos antes
        msleep(25);          // ~25 ms
        elapsed += 25;
    }

    munmap((void*)G, st.st_size);
    return 0;
}
