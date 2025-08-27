#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include "state.h"
#include "sync.h"

static void msleep(int ms) {
    struct timespec ts = { ms/1000, (ms%1000)*1000000L };
    nanosleep(&ts, NULL);
}

int main(void) {
    // adjuntar /game_state (tamaño por fstat) y /game_sync
    int gfd = shm_open(SHM_GAME_STATE, O_RDONLY, 0600);
    if (gfd == -1) { perror("shm_open view"); return 1; }
    struct stat st;
    if (fstat(gfd, &st) == -1) { perror("fstat view"); return 1; }
    size_t GSIZE = (size_t)st.st_size;
    GameState *G = mmap(NULL, GSIZE, PROT_READ, MAP_SHARED, gfd, 0);
    if (G == MAP_FAILED) { perror("mmap view"); return 1; }

    if (sync_attach() != 0) { fprintf(stderr, "sync_attach failed\n"); return 1; }

    // loop de render simple
    for (int frame=0; frame<200; ++frame) {
        rdlock();
        unsigned w = G->w, h = G->h, n = G->n_players;
        printf("\033[H\033[J"); // clear screen
        printf("board %ux%u | players=%u | game_over=%d\n", w, h, n, (int)G->game_over);

        // imprimimos una ventana pequeña (máx 20x10 para no saturar)
        unsigned W = w < 20 ? w : 20;
        unsigned H = h < 10 ? h : 10;
        for (unsigned y=0; y<H; ++y) {
            for (unsigned x=0; x<W; ++x) {
                int v = G->board[idx(G,x,y)];
                int owner = cell_owner(v);
                if (owner >= 0) putchar('A' + owner);       // celda capturada por jugador
                else putchar('0' + (cell_reward(v)%10));     // recompensa 1..9
            }
            putchar('\n');
        }
        for (unsigned i=0; i<n; ++i) {
            printf("P%u pos=(%u,%u) score=%u valid=%u invalid=%u %s\n",
                   i, G->P[i].x, G->P[i].y, G->P[i].score, G->P[i].valids,
                   G->P[i].invalids, G->P[i].blocked ? "[BLOCKED]" : "");
        }
        rdunlock();

        fflush(stdout);
        msleep(150); // ~6-7 fps
    }

    munmap(G, GSIZE);
    close(gfd);
    return 0;
}
