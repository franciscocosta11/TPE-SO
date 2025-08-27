#include "state.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    GameState *g = malloc(state_size(5,5));
    state_zero(g, 5, 5, 3);

    printf("w=%u h=%u n=%u over=%d\n", g->w, g->h, g->n_players, g->game_over);
    printf("P0 score=%u valids=%u invalids=%u blocked=%d\n",
           g->P[0].score, g->P[0].valids, g->P[0].invalids, g->P[0].blocked);
    printf("board[0]=%d board[24]=%d\n", g->board[0], g->board[24]);

    free(g);
    return 0;
}