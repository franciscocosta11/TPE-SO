// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <string.h>

#include "state.h"
#include "state_access.h"
#include "sync.h"

static void msleep(int ms)
{
    struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L}; // MAGIC NUMBER
    nanosleep(&ts, NULL);
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv; /* ancho/alto pueden recibirse por argv; el estado real viene de SHM */
    GameState *G = state_attach();
    if (!G)
    {
        fprintf(stderr, "shm_attach_map view failed\n");
        return 1;
    }

    if (sync_attach() != 0)
    {
        fprintf(stderr, "sync_attach failed\n");
        state_destroy(G);
        return 1;
    }

    for (;;)
    { // WORNING PVS!!!
        view_wait_update_ready();

        rdlock();
        unsigned w = G->w, h = G->h, n = G->n_players;
        printf("\033[H\033[J");
        printf("board %ux%u | players=%u | game_over=%d\n", w, h, n, (int)G->game_over);

        unsigned W = w < 20 ? w : 20;
        unsigned H = h < 10 ? h : 10;
        for (unsigned y = 0; y < H; ++y)
        {
            for (unsigned x = 0; x < W; ++x)
            {
                int v = G->board[idx(G, x, y)];
                int owner = cell_owner(v);
                if (owner >= 0)
                {
                    putchar('A' + owner);
                }
                else
                {
                    int r = cell_reward(v);
                    if (r > 9)
                    {
                        r = 9;
                    }
                    putchar('0' + (r % 10));
                }
            }
            putchar('\n');
        }
        for (unsigned i = 0; i < n; ++i)
        {
            printf("P%u pos=(%u,%u) score=%u valid=%u invalid=%u timeouts=%u %s\n",
                   i, (unsigned)G->P[i].x, (unsigned)G->P[i].y,
                   G->P[i].score, G->P[i].valids, G->P[i].invalids, G->P[i].timeouts,
                   G->P[i].blocked ? "[BLOCKED]" : "");
        }
        bool over = G->game_over;
        rdunlock();

        view_signal_render_complete();
        fflush(stdout);
        if (over)
            break;
        msleep(50);
    }

    state_destroy(G);
    return 0;
}
