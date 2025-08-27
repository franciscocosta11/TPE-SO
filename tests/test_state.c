#include "CuTest.h"
#include "state.h"
#include <stdlib.h>
#include <string.h>


static void Test_state_size(CuTest *tc) {
    size_t s = state_size(10,10);
    /* debe ser al menos header + 100 ints */
    CuAssertTrue(tc, s >= sizeof(GameState) + 100*sizeof(int));
}

static void Test_idx(CuTest *tc) {
    GameState *g = (GameState*)malloc(state_size(10,10));
    g->w = 10; g->h = 10;
    CuAssertIntEquals(tc, 23, idx(g,3,2));  /* 2*10 + 3 */
    free(g);
}

static void Test_state_zero(CuTest *tc) {
    GameState *g = (GameState*)malloc(state_size(5,5));
    state_zero(g, 5,5, 3);

    CuAssertIntEquals(tc, 5, g->w);
    CuAssertIntEquals(tc, 5, g->h);
    CuAssertIntEquals(tc, 3, (int)g->n_players);
    CuAssertIntEquals(tc, 0, g->game_over);

    /* tablero limpio */
    for (int i=0; i<25; i++) {
        CuAssertIntEquals(tc, 0, g->board[i]);
    }

    /* jugador inicializado */
    CuAssertIntEquals(tc, 0, g->P[0].score);
    CuAssertStrEquals(tc, "", g->P[0].name);

    free(g);
}

static void Test_board_fill_rewards_range(CuTest *tc) {
    GameState *g = (GameState*)malloc(state_size(8,8));
    state_zero(g, 8, 8, 3);
    board_fill_rewards(g, 1234);

    for (int i = 0; i < 64; ++i) {
        CuAssertTrue(tc, g->board[i] >= 1 && g->board[i] <= 9);
    }
    free(g);
}

static void Test_board_fill_rewards_determinism(CuTest *tc) {
    GameState *a = (GameState*)malloc(state_size(6,6));
    GameState *b = (GameState*)malloc(state_size(6,6));
    GameState *c = (GameState*)malloc(state_size(6,6));

    state_zero(a, 6,6, 2);
    state_zero(b, 6,6, 2);
    state_zero(c, 6,6, 2);

    board_fill_rewards(a, 777);
    board_fill_rewards(b, 777);
    board_fill_rewards(c, 778);

    // misma seed => mismo tablero
    CuAssertIntEquals(tc, 0, memcmp(a->board, b->board, 36 * sizeof(int)));

    // seed distinta => al menos una celda distinta
    int diff = memcmp(a->board, c->board, 36 * sizeof(int));
    CuAssertTrue(tc, diff != 0);

    free(a); free(b); free(c);
}

static void Test_players_place_grid_basic(CuTest *tc) {
    GameState *g = (GameState*)malloc(state_size(12, 8));
    state_zero(g, 12, 8, 3);
    board_fill_rewards(g, 42);
    players_place_grid(g);

    /* Anclas esperadas para 12x8:
       xs = {2,6,10}, ys = {1,4,6}
       i=0 -> (2,1), i=1 -> (6,1), i=2 -> (10,1)  (siempre que estén libres)
     */
    CuAssertIntEquals(tc, 2,  g->P[0].x);
    CuAssertIntEquals(tc, 1,  g->P[0].y);
    CuAssertIntEquals(tc, 6,  g->P[1].x);
    CuAssertIntEquals(tc, 1,  g->P[1].y);
    CuAssertIntEquals(tc, 10, g->P[2].x);
    CuAssertIntEquals(tc, 1,  g->P[2].y);

    /* Celdas capturadas correctamente */
    CuAssertIntEquals(tc, 0, cell_owner(g->board[idx(g, g->P[0].x, g->P[0].y)]));
    CuAssertIntEquals(tc, 1, cell_owner(g->board[idx(g, g->P[1].x, g->P[1].y)]));
    CuAssertIntEquals(tc, 2, cell_owner(g->board[idx(g, g->P[2].x, g->P[2].y)]));

    free(g);
}

static void Test_players_place_grid_small_board_no_overlap(CuTest *tc) {
    GameState *g = (GameState*)malloc(state_size(2, 2));
    state_zero(g, 2, 2, 4);
    board_fill_rewards(g, 7);
    players_place_grid(g);

    /* Las 4 posiciones deben ser únicas y dentro de [0..1]x[0..1] */
    int used[4] = {0,0,0,0};
    for (int i=0; i<4; ++i) {
        CuAssertTrue(tc, g->P[i].x < 2 && g->P[i].y < 2);
        int id = g->P[i].y*2 + g->P[i].x;
        CuAssertIntEquals(tc, 0, used[id]);
        used[id] = 1;

        /* Cada celda capturada por su dueño */
        CuAssertIntEquals(tc, i, cell_owner(g->board[idx(g, g->P[i].x, g->P[i].y)]));
    }
    free(g);
}

static void Test_players_place_grid_determinism(CuTest *tc) {
    GameState *a = (GameState*)malloc(state_size(9, 9));
    GameState *b = (GameState*)malloc(state_size(9, 9));
    state_zero(a, 9, 9, 5);
    state_zero(b, 9, 9, 5);
    board_fill_rewards(a, 123);
    board_fill_rewards(b, 123);
    players_place_grid(a);
    players_place_grid(b);

    /* Misma w,h,n y misma seed => mismas posiciones */
    CuAssertIntEquals(tc, a->P[0].x, b->P[0].x);
    CuAssertIntEquals(tc, a->P[0].y, b->P[0].y);
    CuAssertIntEquals(tc, a->P[4].x, b->P[4].x);
    CuAssertIntEquals(tc, a->P[4].y, b->P[4].y);
    free(a); free(b);
}


/* --- suite --- */
CuSuite* StateGetSuite() {
    CuSuite* suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, Test_state_size);
    SUITE_ADD_TEST(suite, Test_idx);
    SUITE_ADD_TEST(suite, Test_state_zero);
    SUITE_ADD_TEST(suite, Test_board_fill_rewards_range);
    SUITE_ADD_TEST(suite, Test_board_fill_rewards_determinism);
    SUITE_ADD_TEST(suite, Test_players_place_grid_basic);
    SUITE_ADD_TEST(suite, Test_players_place_grid_small_board_no_overlap);
    SUITE_ADD_TEST(suite, Test_players_place_grid_determinism);
    return suite;
}



