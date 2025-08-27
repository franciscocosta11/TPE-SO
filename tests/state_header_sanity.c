#include "state.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    size_t SZ = state_size(10, 10);
    GameState *g = (GameState*)malloc(SZ);
    g->w = 10; g->h = 10;
    printf("size=%zu idx(3,2)=%d capturedBy0=%d owner=%d\n",
           SZ, idx(g,3,2), make_captured(0), cell_owner(make_captured(0)));
    free(g);
    return 0;
}