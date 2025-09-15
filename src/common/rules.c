// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "rules.h"
#define DIRECTIONS 8 

static void dir_delta(Dir d, int *dx, int *dy) {
    static const int DX[DIRECTIONS] = { 0, 1, 1, 1, 0,-1,-1,-1 };
    static const int DY[DIRECTIONS] = {-1,-1, 0, 1, 1, 1, 0,-1 };
    *dx = DX[d]; *dy = DY[d];
}

int rules_validate(const GameState *g, int pid, Dir d, int *gain) {
    // entradas validas
    if (!g) return 0;
    if (pid < 0 || (unsigned)pid >= g->n_players) return 0;
    if ((int)d < 0 || (int)d > 7) return 0; /* validar dirección */

    int dx, dy; dir_delta(d, &dx, &dy);

    int x = (int)g->P[pid].x + dx;
    int y = (int)g->P[pid].y + dy;

    if (x < 0 || y < 0 || x >= (int)g->w || y >= (int)g->h) return 0;

    int v = g->board[idx(g, (unsigned)x, (unsigned)y)];

    // chequeo que no este capturada por nadie
    if (cell_owner(v) != -1) return 0;  // ya capturada por alguien

    // ya se que no es un player, ahora me fijo si es un valor en el rango aceptado [1,9]
    int r = cell_reward(v);
    if ( r > 9) return 0; 

    if (gain) *gain = r;   /* 0..9 */
    return 1;
}

void rules_apply(GameState *g, int pid, Dir d) {
    /* validar nuevamente para evitar aplicar movimientos corruptos */
    if (!g) return;
    int gain = 0;
    if (!rules_validate(g, pid, d, &gain)) return;

    int dx, dy; dir_delta(d, &dx, &dy);

    int nx = (int)g->P[pid].x + dx;
    int ny = (int)g->P[pid].y + dy;
    /* re-leer v/r ya validados por rules_validate */
    int v  = g->board[idx(g, (unsigned)nx, (unsigned)ny)];
    int r  = cell_reward(v);

    /* mover */
    g->P[pid].x = (unsigned short)nx;
    g->P[pid].y = (unsigned short)ny;

    /* capturar la celda */
    g->board[idx(g, (unsigned)nx, (unsigned)ny)] = make_captured(pid);

    /* puntaje y contadores */
    g->P[pid].score  += (unsigned)r;
    g->P[pid].valids += 1;
}

int player_can_move(const GameState *g, int pid) {
    if (pid < 0 || (unsigned)pid >= g->n_players) return 0;
    for (int d = 0; d < DIRECTIONS; ++d) { 
        if (rules_validate(g, pid, (Dir)d, NULL)) return 1;
    }
    return 0;
}
