#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "time.h"
#include <stdbool.h>
#include <sys/mman.h> // For PROT_READ



#include "shm.h"
#include "state.h"
#include "state_access.h"
#include "sync.h"

// Ensure Dir type is defined or included
#include "rules.h" // Assuming Dir is defined here; adjust if needed

static int find_self_index(const GameState *G, pid_t me) {
    for (unsigned i = 0; i < G->n_players; ++i) if (G->P[i].pid == me) return (int)i;
    return -1;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    size_t gsz = 0;
    GameState *G = (GameState*)shm_attach_map(SHM_GAME_STATE, &gsz, PROT_READ);
    if (!G) return 1;
    if (sync_attach() != 0) return 1;

    pid_t me = getpid();
    int my = -1;

    /* Esperar a que el master setee mi PID en el estado */
    for (int tries = 0; tries < 200 && my < 0; ++tries) {
        state_read_begin();
        my = find_self_index(G, me);
        bool over = G->game_over;
        state_read_end();
        if (over) return 0;
    if (my < 0) {
        struct timespec ts = { .tv_sec = 0, .tv_nsec = 50 * 1000 * 1000 }; // 50 ms
nanosleep(&ts,NULL);
    }
    }
    if (my < 0) return 0;

    while (1) {
        /* Esperar turno con polling suave para poder salir si termina el juego */
        int got_turn = 0;
        for (;;) {
            int r = player_wait_turn_timed(my, 150); /* 150 ms */
            if (r == 1) { got_turn = 1; break; }
            if (r < 0)  { /* error raro */ break; }
            /* r==0: timeout corto, revisar si terminó el juego */
            state_read_begin();
            bool over = G->game_over;
            bool b    = G->P[my].blocked;
            state_read_end();
            if (over || b) { got_turn = 0; break; }
        }
        if (!got_turn) break; /* juego terminó o me bloquearon */

        /* Elegir un movimiento legal y con mayor recompensa adyacente */
        uint8_t best_dir = 0;
        int best_gain = -1;
        int can_play = 0;

        state_read_begin();
        if (G->game_over || G->P[my].blocked) { state_read_end(); break; }
        for (int d = 0; d < 8; ++d) {
            int gain = 0;
            if (rules_validate(G, my, (Dir)d, &gain)) {
                can_play = 1;
                if (gain > best_gain) { best_gain = gain; best_dir = (uint8_t)d; }
            }
        }
        state_read_end();

        if (can_play) {
            ssize_t wres = write(1, &best_dir, 1);
            (void)wres; // Suppress unused-result warning
        } else {
            /* No tengo jugada legal: envío PASS sentinel 0xFF y termino */
            uint8_t pass = 0xFF;
            ssize_t wpass = write(1, &pass, 1);
            (void)wpass;
            break;
        }
    }

    return 0;
}
