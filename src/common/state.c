#include "state.h"
#include <sys/mman.h>

int idx(const GameState *g, unsigned x, unsigned y) {
    return (int)(y * g->w + x);
}

size_t state_size(unsigned w, unsigned h) {
    return sizeof(GameState) + (size_t)w * (size_t)h * sizeof(int);
}

void state_zero(GameState *g, unsigned w, unsigned h, unsigned n_players) {
    g->w = w;
    g->h = h;
    g->n_players = n_players;
    g->game_over = false;

    for (unsigned i = 0; i < MAX_PLAYERS; i++) {
        Player *p = &g->P[i];
        p->name[0] = '\0';
        p->score = 0;
        p->valids = 0;
        p->invalids = 0;
        p->timeouts = 0;         /* NUEVO */
        p->x = 0;
        p->y = 0;
        p->pid = 0;
        p->blocked = false;
    }

    size_t cells = (size_t)w * (size_t)h;
    memset(g->board, 0, cells * sizeof(int));
}

void board_fill_rewards(GameState *g, unsigned seed) {
    srand(seed);
    size_t cells = (size_t)g->w * (size_t)g->h;
    for (size_t i = 0; i < cells; ++i) {
        g->board[i] = 1 + rand() % 9;
    }
}

static inline int in_bounds(const GameState *g, int x, int y) {
    return (x >= 0 && y >= 0 && x < g->w && y < g->h);
}

static inline int cell_is_free_for_spawn(const GameState *g, int x, int y) {
    int v = g->board[idx(g, (unsigned)x, (unsigned)y)];
    return cell_owner(v) == -1;
}

static void find_nearest_free(const GameState *g, int x0, int y0, int *outx, int *outy) {
    int maxr = (g->w > g->h ? g->w : g->h);
    for (int r = 0; r <= maxr; ++r) {
        for (int yy = y0 - r; yy <= y0 + r; ++yy) {
            for (int xx = x0 - r; xx <= x0 + r; ++xx) {
                if (!in_bounds(g, xx, yy)) continue;
                if (cell_is_free_for_spawn(g, xx, yy)) {
                    *outx = xx; *outy = yy;
                    return;
                }
            }
        }
    }
    *outx = x0; *outy = y0;
}

void players_place_grid(GameState *g) {
    int xs[3] = { (g->w * 1) / 6, (g->w * 3) / 6, (g->w * 5) / 6 };
    int ys[3] = { (g->h * 1) / 6, (g->h * 3) / 6, (g->h * 5) / 6 };

    for (int i = 0; i < 3; ++i) {
        if (xs[i] < 0) xs[i] = 0;
        if (ys[i] < 0) ys[i] = 0;
        if (xs[i] >= g->w) xs[i] = g->w - 1;
        if (ys[i] >= g->h) ys[i] = g->h - 1;
    }

    unsigned np = g->n_players;
    if (np > MAX_PLAYERS) np = MAX_PLAYERS;

    for (unsigned i = 0; i < np; ++i) {
        int row = (int)(i / 3);
        int col = (int)(i % 3);
        if (row > 2) row = 2;
        int tx = xs[col];
        int ty = ys[row];

        int px = tx, py = ty;
        if (!cell_is_free_for_spawn(g, px, py)) {
            find_nearest_free(g, tx, ty, &px, &py);
        }

        g->P[i].x = (unsigned short)px;
        g->P[i].y = (unsigned short)py;
        g->P[i].blocked = false;
        g->board[idx(g, px, py)] = make_captured((int)i);
    }
}

GameState* state_create(unsigned w, unsigned h) {
    size_t size = state_size(w, h);
    return (GameState*)shm_create_map(SHM_GAME_STATE, size, PROT_READ | PROT_WRITE);
}

GameState* state_attach(void) {
    return (GameState*)shm_attach_map(SHM_GAME_STATE, NULL, PROT_READ | PROT_WRITE);
}

void state_destroy(GameState *g) {
    if (g == NULL) return;
    size_t size = state_size(g->w, g->h);
    munmap(g, size);
}
