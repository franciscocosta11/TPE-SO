#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "log.h"
#include "state.h"

/* Nombre canónico del área de memoria compartida del estado */
#ifndef GAME_STATE_SHM_NAME
#define GAME_STATE_SHM_NAME "/game_state"
#endif

/* Render mínimo ASCII:
 *  - '.' libre (0)
 *  - '1'..'9' recompensa (>0, saturado a 9)
 *  - 'X' capturada (<0)
 *  - opcional: '0'..'7' si hay un jugador en (x,y)
 * En esta versión no computamos posiciones de jugadores si P[] no está poblado.
 */
static void render_board(const GameState *gs) {
    printf("=== TABLERO === (%dx%d) jugadores=%d game_over=%d\n",
           gs->w, gs->h, gs->n_players, gs->game_over);

    for (int y = 0; y < gs->h; y++) {
        for (int x = 0; x < gs->w; x++) {
            int val = gs->board ? gs->board[idx(gs, x, y)] : 0;
            char c;
            if (val == 0) {
                c = '.';
            } else if (val > 0) {
                if (val > 9) val = 9;
                c = (char)('0' + val);
            } else { // val < 0
                c = 'X';
            }
            putchar(c);
            putchar(' ');
        }
        putchar('\n');
    }

    // Stats (si P[] está disponible)
    for (int i = 0; i < gs->n_players && i < 8; i++) {
        printf("P%d: x=%d y=%d score=%d alive=%d\n",
               i, gs->P[i].x, gs->P[i].y, gs->P[i].score, gs->P[i].alive);
    }
}

/* Crea un estado dummy en heap cuando no hay shm disponible, para probar la vista. */
static GameState *make_dummy_state(int w, int h, int n_players) {
    GameState *gs = calloc(1, sizeof(GameState));
    if (!gs) return NULL;
    gs->w = w;
    gs->h = h;
    gs->n_players = n_players;
    gs->game_over = 0;
    gs->board = calloc((size_t)w * (size_t)h, sizeof(int32_t));
    if (!gs->board) {
        free(gs);
        return NULL;
    }
    // Relleno de ejemplo: diagonal con recompensas y una celda capturada
    for (int i = 0; i < w && i < h; i++) {
        gs->board[idx(gs, i, i)] = (i % 9) + 1; // 1..9
    }
    if (w > 2 && h > 1) {
        gs->board[idx(gs, 2, 1)] = -1; // capturada
    }
    // Posiciones de ejemplo
    if (n_players > 0) { gs->P[0].x = 0; gs->P[0].y = 0; gs->P[0].alive = 1; }
    if (n_players > 1) { gs->P[1].x = w-1; gs->P[1].y = h-1; gs->P[1].alive = 1; }
    return gs;
}

/* Libera el estado dummy */
static void free_dummy_state(GameState *gs) {
    if (!gs) return;
    free(gs->board);
    free(gs);
}

int main(void) {
    log_info("Vista iniciando...\n");

    // Intentar abrir la shm real del estado
    int fd = shm_open(GAME_STATE_SHM_NAME, O_RDONLY, 0);
    if (fd < 0) {
        log_err("No se pudo abrir %s (%s). Usando estado dummy para debug.\n",
                GAME_STATE_SHM_NAME, strerror(errno));

        GameState *dummy = make_dummy_state(8, 6, 2);
        if (!dummy) {
            log_err("No se pudo crear estado dummy.\n");
            return 1;
        }
        render_board(dummy);
        free_dummy_state(dummy);
        return 0;
    }

    // Mapear tamaño completo del objeto shm
    struct stat st;
    if (fstat(fd, &st) < 0) {
        log_err("fstat falló sobre shm (%s)\n", strerror(errno));
        close(fd);
        return 1;
    }
    if (st.st_size < (off_t)sizeof(GameState)) {
        log_err("shm más chica que GameState (%ld < %zu)\n",
                (long)st.st_size, sizeof(GameState));
        close(fd);
        return 1;
    }

    void *base = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (base == MAP_FAILED) {
        log_err("mmap falló (%s)\n", strerror(errno));
        close(fd);
        return 1;
    }
    close(fd); // fd no se necesita más tras el mmap

    const GameState *gs = (const GameState *)base;

    // En Día 2 leemos sin locks. En Día 3, rodear con rdlock/rdunlock.
    log_info("Vista mapeó estado real. w=%d h=%d n_players=%d\n",
             gs->w, gs->h, gs->n_players);

    render_board(gs);

    // Desmapear
    if (munmap((void *)gs, (size_t)st.st_size) < 0) {
        log_err("munmap falló (%s)\n", strerror(errno));
        return 1;
    }

    log_info("Vista finalizada OK.\n");
    return 0;
}
