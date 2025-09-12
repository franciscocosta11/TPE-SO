// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <sys/mman.h> // For PROT_READ

#include "shm.h"
#include "state.h"
#include "state_access.h"
#include "sync.h"

// Ensure Dir type is defined or included
#include "rules.h" // Assuming Dir is defined here; adjust if needed

#define MAX_INIT_TRIES 200           // Maximum number of attempts to find self in game state
#define INIT_POLL_DELAY_MS 50        // Delay between init attempts in milliseconds
#define TURN_WAIT_TIMEOUT_MS 150     // Timeout for waiting for turn
#define PASS_SENTINEL 0xFF           // Value sent when no legal moves are available
#define POLL_DELAY_MS 50             // General polling delay
#define NANOSEC_PER_MS 1000000L      // Number of nanoseconds in a millisecond

static int find_self_index(const GameState *G, pid_t me)
{
    for (unsigned i = 0; i < G->n_players; ++i)
        if (G->P[i].pid == me)
            return (int)i;
    return -1;
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, 0);

    GameState *G = state_attach(); // REVISAR ACCESO A MEM
    if (!G)
        return 1;
    if (sync_attach() != 0)
        return 1;

    unsigned argW = 0, argH = 0;
    if (argc >= 3)
    {
        argW = (unsigned)strtoul(argv[1], NULL, 10);
        argH = (unsigned)strtoul(argv[2], NULL, 10);
    }

    pid_t me = getpid();
    int my = -1;

    /* Esperar a que el master setee mi PID en el estado */
    for (int tries = 0; tries < MAX_INIT_TRIES && my < 0; ++tries)
    {
        state_read_begin();
        my = find_self_index(G, me);
        bool over = G->game_over;
        /* Advertir una única vez si el tamaño indicado por argv difiere del SHM */
        static int warned_size = 0;
        if (!warned_size && argW && argH && (G->w != argW || G->h != argH))
        {
            fprintf(stderr, "player: aviso: tamaño SHM=%ux%u difiere de argv=%ux%u\n",
                    G->w, G->h, argW, argH);
            warned_size = 1;
        }
        state_read_end();
        if (over)
            return 0;
        if (my < 0)
        {
            struct timespec ts = {.tv_sec = 0, .tv_nsec = INIT_POLL_DELAY_MS * NANOSEC_PER_MS};
            nanosleep(&ts, NULL);
        }
    }
    if (my < 0)
        return 0;

    while (1)
    {
        /* Esperar turno con polling suave para poder salir si termina el juego */
        int got_turn = 0;
        for (;;)
        {
            int r = player_wait_turn_timed(my, TURN_WAIT_TIMEOUT_MS);
            if (r == 1)
            {
                got_turn = 1;
                break;
            }
            if (r < 0)
            { /* error raro */
                break;
            }
            /* r==0: timeout corto, revisar si terminó el juego */
            state_read_begin();
            bool over = G->game_over;
            bool b = G->P[my].blocked;
            state_read_end();
            if (over || b)
            {
                got_turn = 0;
                break;
            }
        }
        if (!got_turn)
            break; /* juego terminó o me bloquearon */

        /* Elegir un movimiento legal y con mayor recompensa adyacente */
        uint8_t best_dir = 0;
        int best_gain = -1;
        int can_play = 0;

        state_read_begin();
        if (G->game_over || G->P[my].blocked)
        {
            state_read_end();
            break;
        }
        for (int d = 0; d < 8; ++d) // MAGIC NUMBER
        {
            int gain = 0;
            if (rules_validate(G, my, (Dir)d, &gain))
            {
                can_play = 1;
                if (gain > best_gain)
                {
                    best_gain = gain;
                    best_dir = (uint8_t)d;
                }
            }
        }
        state_read_end();

        if (can_play)
        {
            ssize_t wres = write(1, &best_dir, 1);
            (void)wres; // Suppress unused-result warning
        }
        else
        {
            /* No tengo jugada legal: envío PASS sentinel y espero a que el master me marque bloqueo o termine */
            uint8_t pass = PASS_SENTINEL;
            ssize_t wpass = write(1, &pass, 1);
            (void)wpass;
            /* esperar polling corto hasta que game_over o blocked */
            for (;;)
            {
                state_read_begin();
                bool over = G->game_over;
                bool b = G->P[my].blocked;
                state_read_end();
                if (over || b)
                    break;
                struct timespec ts = {.tv_sec = 0, .tv_nsec = POLL_DELAY_MS * NANOSEC_PER_MS};
                nanosleep(&ts, NULL);
            }
            break;
        }
    }

    return 0;
}
