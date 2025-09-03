// tests/init_state.c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>  


#include "state.h"
#include "sync.h"
#include "state_access.h"
#include "shm.h"

int main(void) {
    unsigned W = 12, H = 8, N = 3;
    size_t GSIZE = state_size(W, H);

    // Crear y mapear /game_state RW (no se unlinkea aquí)
    GameState *G = (GameState*)shm_create_map(SHM_GAME_STATE, GSIZE, PROT_READ | PROT_WRITE);
    if (!G) { fprintf(stderr, "[init] shm_create_map(/game_state) failed\n"); return 1; }

    // Crear /game_sync (semáforos compartidos)
    if (sync_create() != 0) {
        fprintf(stderr, "[init] sync_create failed\n");
        return 1;
    }

    // Inicializar el estado bajo lock de escritura
    state_write_begin();
    state_zero(G, W, H, N);
    board_fill_rewards(G, 42);
    players_place_grid(G);
    G->game_over = false;
    state_write_end();

    printf("[init] /game_state listo (W=%u H=%u N=%u). No se destruye nada.\n", W, H, N);
    // Importante: NO llamar a sync_destroy() ni shm_unlink() aquí.
    // El writer/reader/view se adjuntan y usan esto.

    return 0;
}
