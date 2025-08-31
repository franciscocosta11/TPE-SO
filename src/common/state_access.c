#include "state_access.h"

unsigned state_get_w(const GameState *G) { return G->w; }
unsigned state_get_h(const GameState *G) { return G->h; }
unsigned state_get_n(const GameState *G) { return G->n_players; }
bool     state_is_over(const GameState *G) { return G->game_over; }

int state_remaining_rewards(const GameState *G) {
    int cnt = 0;
    for (unsigned y = 0; y < G->h; ++y)
        for (unsigned x = 0; x < G->w; ++x)
            if (cell_reward(G->board[idx(G,x,y)]) > 0) cnt++;
    return cnt;
}
