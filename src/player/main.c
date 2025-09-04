// src/player/main.c
#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <sys/mman.h>

#include "shm.h"
#include "state.h"
#include "state_access.h"
#include "sync.h"

static int find_self_index(const GameState *G, pid_t me) {
    for (unsigned i = 0; i < G->n_players; ++i) {
        if (G->P[i].pid == me) return (int)i;
    }
    return -1;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    size_t gsz = 0;
    GameState *G = (GameState*)shm_attach_map(SHM_GAME_STATE, &gsz, PROT_READ);
    if (!G) return 1;
    if (sync_attach() != 0) return 1;

    /* Esperar a que el master registre nuestro PID */
    pid_t me = getpid();
    int my = -1;
    for (int tries = 0; tries < 100 && my < 0; ++tries) {
        state_read_begin();
        my = find_self_index(G, me);
        bool over = G->game_over;
        state_read_end();
        if (over) return 0;
        if (my < 0) {
            struct timespec ts = {0, 50 * 1000 * 1000L};
            nanosleep(&ts, NULL);
        }
    }
    if (my < 0) return 0; // no nos encontraron

    /* Loop principal: elegir un movimiento legal con mayor recompensa cercana */
    while (1) {
        uint8_t best_dir = 0;
        int best_gain = -1;
        int can_play = 0;
        int blocked = 0;

        state_read_begin();
        if (G->game_over) { state_read_end(); break; }

        blocked = G->P[my].blocked ? 1 : 0;
        if (!blocked) {
            for (int d = 0; d < 8; ++d) {
                int gain = 0;
                if (rules_validate(G, my, (Dir)d, &gain)) {
                    can_play = 1;
                    if (gain > best_gain) {
                        best_gain = gain;
                        best_dir = (uint8_t)d;
                    }
                }
            }
        }
        state_read_end();

        if (blocked) break;         // ya sin movimientos
        if (!can_play) {            // no hay legales ahora; reintento
            struct timespec ts = {0, 100 * 1000 * 1000L};
            nanosleep(&ts, NULL);
            continue;
        }

        // Emitir exactamente 1 byte (la jugada)
        ssize_t wr = write(1, &best_dir, 1);
        (void)wr;

        // Pequeño delay para no acumular jugadas antes de la siguiente ronda
        {
            struct timespec ts = {0, 120 * 1000 * 1000L};
            nanosleep(&ts, NULL);
        }
    }

    return 0;
}
