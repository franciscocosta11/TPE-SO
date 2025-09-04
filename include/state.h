#ifndef STATE_H
#define STATE_H

#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "shm.h"

#define MAX_PLAYERS 9
#define NAME_LEN    16
#define SHM_GAME_STATE "/game_state"

typedef enum {
    DIR_N=0, DIR_NE=1, DIR_E=2, DIR_SE=3,
    DIR_S=4, DIR_SW=5, DIR_W=6, DIR_NW=7
} Dir;

typedef struct Player {
    char name[NAME_LEN];
    unsigned score, invalids, valids;
    unsigned timeouts;              /* NUEVO: cantidad de turnos vencidos por timeout */
    unsigned short x,y;
    pid_t pid;
    bool blocked;
} Player;

typedef struct GameState {
    unsigned short w,h;
    unsigned n_players;
    Player P[MAX_PLAYERS];
    bool game_over;
    int board[];
} GameState;

GameState* state_create(unsigned w, unsigned h);
GameState* state_attach(void);
void state_destroy(GameState *g);

int  rules_validate(const GameState *g, int pid, Dir d, int *gain);
void rules_apply(GameState *g, int pid, Dir d);

void state_zero(GameState* g, unsigned w, unsigned h, unsigned n);
size_t state_size(unsigned w, unsigned h);
int idx(const GameState *g, unsigned x, unsigned y);
void board_fill_rewards(GameState *g, unsigned seed);
void players_place_grid(GameState *g);
int player_can_move(const GameState *g, int pid);

static inline int cell_reward(int v)         { return v > 0 ? v : 0; }
static inline int cell_owner(int v)          { return v < 0 ? (-v - 1) : -1; }
static inline int make_captured(int owner_i) { return -(owner_i + 1); }

#endif // STATE_H
