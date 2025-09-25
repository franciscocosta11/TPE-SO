#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <limits.h>
#include "state.h"
#include "state_access.h"
#include "sync.h"
#include "rules.h"

#define MAX_INIT_TRIES 200
#define INIT_POLL_DELAY_MS 50
#define TURN_WAIT_TIMEOUT_MS 150
#define PASS_SENTINEL 0xFF
#define POLL_DELAY_MS 50
#define NANOSEC_PER_MS 1000000L

#define EDGE_SAFE_MARGIN 2
#define EDGE_PENALTY     2

#define FREE_RADIUS 4
#define W_GAIN_BASE     50
#define W_ALIGN_BASE     1
#define ALIGN_DIV       50
#define W_SPACE_BASE     1
#define W_ENEMY_FEW      3
#define W_ENEMY_MANY     6
#define W_CENTER_LARGE   1

static const int NDX[8] = { 0, +1, +1, +1,  0, -1, -1, -1};
static const int NDY[8] = {-1, -1,  0, +1, +1, +1,  0, -1};

/* cuenta celdas libres alcanzables en ventana (nx,ny) con radio R (8-conectado) */
static int free_space_window(const GameState *G, int nx, int ny, int R)
{
    const int W = (int)G->w, H = (int)G->h;
    if (R > FREE_RADIUS) R = FREE_RADIUS;
    const int winR = 2*R + 1;
    const int area = winR * winR;
    const int wx0 = nx - R, wy0 = ny - R;
    enum { MAXQ = (2*FREE_RADIUS + 1) * (2*FREE_RADIUS + 1) };
    int qx[MAXQ], qy[MAXQ];
    unsigned char vis[MAXQ];
    for (int i = 0; i < MAXQ; ++i) vis[i] = 0;

    if (nx < 0 || ny < 0 || nx >= W || ny >= H) return 0;
    if (cell_owner(G->board[idx(G, (unsigned)nx, (unsigned)ny)]) != -1) return 0;
    if (!(nx >= wx0 && nx <= wx0 + (winR - 1) && ny >= wy0 && ny <= wy0 + (winR - 1))) return 0;

    int head = 0, tail = 0, count = 0;
    int si = (ny - wy0) * winR + (nx - wx0);
    vis[si] = 1;
    qx[tail] = nx; qy[tail] = ny; tail = (tail + 1) % area;
    count = 1;
    while (head != tail)
    {
        int cx = qx[head], cy = qy[head];
        head = (head + 1) % area;
        for (int k = 0; k < 8; ++k)
        {
            int tx = cx + NDX[k], ty = cy + NDY[k];
            if (tx < 0 || ty < 0 || tx >= W || ty >= H) continue;
            if (!(tx >= wx0 && tx <= wx0 + (winR - 1) && ty >= wy0 && ty <= wy0 + (winR - 1))) continue;
            if (cell_owner(G->board[idx(G, (unsigned)tx, (unsigned)ty)]) != -1) continue;
            int wi = (ty - wy0) * winR + (tx - wx0);
            if (wi < 0 || wi >= area) continue;
            if (vis[wi]) continue;
            vis[wi] = 1;
            qx[tail] = tx; qy[tail] = ty; tail = (tail + 1) % area;
            if (tail == head) break;
            ++count;
        }
    }
    return count;
}

/* vector global hacia zonas con recompensa, ponderado por distancia */
static void reward_vector(const GameState *G, int x, int y, int *out_vx, int *out_vy)
{
    int vx = 0, vy = 0;
    const int W = (int)G->w, H = (int)G->h;
    for (int cy = 0; cy < H; ++cy)
    {
        for (int cx = 0; cx < W; ++cx)
        {
            int v = G->board[idx(G, (unsigned)cx, (unsigned)cy)];
            if (cell_owner(v) != -1) continue;
            int r = cell_reward(v);
            if (r <= 0) continue;
            int ddx = cx - x, ddy = cy - y;
            int dist = abs(ddx) + abs(ddy);
            int w = (r * 10) / (1 + dist);
            vx += ddx * w;
            vy += ddy * w;
        }
    }
    *out_vx = vx; *out_vy = vy;
}

static int find_self_index(const GameState *G, pid_t me)
{
    for (unsigned i = 0; i < G->n_players; ++i)
        if (G->P[i].pid == me)
            return (int)i;
    return -1;
}

static void parse_dims(int argc, char *argv[], unsigned *w, unsigned *h)
{
    *w = 0;
    *h = 0;
    if (argc >= 3)
    {
        *w = (unsigned)strtoul(argv[1], NULL, 10);
        *h = (unsigned)strtoul(argv[2], NULL, 10);
    }
}

static int wait_for_turn_or_end(GameState *G, int my)
{
    for (;;)
    {
        int r = player_wait_turn_timed(my, TURN_WAIT_TIMEOUT_MS);
        if (r == 1)
            return 1;
        if (r < 0)
            return 0;
        state_read_begin();
        bool over = G->game_over;
        bool b = G->P[my].blocked;
        state_read_end();
        if (over || b)
            return 0;
    }
}

static int choose_best_move(GameState *G, int my, uint8_t *out_dir)
{
    long long best_score = LLONG_MIN;
    int best_gain = -1;
    uint8_t best_dir = 0;

    const Player *me = &G->P[my];
    const int x = (int)me->x;
    const int y = (int)me->y;
    const int W = (int)G->w;
    const int H = (int)G->h;
    const unsigned N = G->n_players;

    const int W_GAIN = W_GAIN_BASE;
    const int W_ALIGN = W_ALIGN_BASE * ((W*H) >= 200 ? 3 : 2);
    const int W_SPACE = W_SPACE_BASE * (N >= 6 ? 2 : 1);
    const int W_ENEMY = (N >= 6 ? W_ENEMY_MANY : W_ENEMY_FEW);
    const int W_CENTER = ((W*H) >= 200 ? W_CENTER_LARGE : 0);

    int gvx = 0, gvy = 0;
    reward_vector(G, x, y, &gvx, &gvy);

    for (int d = 0; d < 8; ++d)
    {
        int gain = 0;
        if (!rules_validate(G, my, (Dir)d, &gain))
            continue;

        int nx = x + NDX[d];
        int ny = y + NDY[d];
    if (nx < 0) nx = 0;
    if (nx >= W) nx = W - 1;
    if (ny < 0) ny = 0;
    if (ny >= H) ny = H - 1;

        int dist_left = nx;
        int dist_right = (W - 1) - nx;
        int dist_top = ny;
        int dist_bottom = (H - 1) - ny;
        int dist_edge = dist_left;
        if (dist_right < dist_edge) dist_edge = dist_right;
        if (dist_top < dist_edge) dist_edge = dist_top;
        if (dist_bottom < dist_edge) dist_edge = dist_bottom;
        int penalty = 0;
        if (dist_edge < EDGE_SAFE_MARGIN)
            penalty = (EDGE_SAFE_MARGIN - dist_edge) * EDGE_PENALTY;

        int space = free_space_window(G, nx, ny, FREE_RADIUS);

        int dmin = 1000000;
        for (unsigned k = 0; k < N; ++k)
        {
            if ((int)k == my) continue;
            int px = (int)G->P[k].x;
            int py = (int)G->P[k].y;
            int ddx = abs(px - nx), ddy = abs(py - ny);
            int cheb = ddx > ddy ? ddx : ddy;
            if (cheb < dmin) dmin = cheb;
        }
        int enemy_threat = 0;
        if (dmin <= 2) enemy_threat = 3 - dmin; 

        int align = NDX[d]*gvx + NDY[d]*gvy;

        int cx = (W - 1) / 2;
        int cy = (H - 1) / 2;
        int dcx = abs(nx - cx), dcy = abs(ny - cy);
        int dcenter = dcx > dcy ? dcx : dcy;

        long long score = 0;
        score += (long long)W_GAIN * gain;
        score -= (long long)penalty;
        score += (long long)W_SPACE * space;
        score -= (long long)W_ENEMY * enemy_threat;
        score += (long long)W_ALIGN * (align / ALIGN_DIV);
        score -= (long long)W_CENTER * dcenter;

        if (score > best_score || (score == best_score && gain > best_gain))
        {
            best_score = score;
            best_gain = gain;
            best_dir = (uint8_t)d;
        }
    }

    if (best_gain >= 0)
    {
        *out_dir = best_dir;
        return 1;
    }
    return 0;
}

static void send_pass_and_wait(GameState *G, int my)
{
    uint8_t pass = PASS_SENTINEL;
    ssize_t wr = write(1, &pass, 1);
    (void)wr;
    while (1)
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
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, 0);

    GameState *G = state_attach();
    if (!G)
        return 1;
    if (sync_attach() != 0)
        return 1;

    unsigned argW, argH;
    parse_dims(argc, argv, &argW, &argH);

    pid_t me = getpid();
    int my = -1;

    for (int tries = 0; tries < MAX_INIT_TRIES && my < 0; ++tries)
    {
        state_read_begin();
        my = find_self_index(G, me);
        bool over = G->game_over;
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
        int got_turn = wait_for_turn_or_end(G, my);
        if (!got_turn)
            break;

        state_read_begin();
        if (G->game_over || G->P[my].blocked)
        {
            state_read_end();
            break;
        }
        uint8_t best_dir = 0;
        int can_play = choose_best_move(G, my, &best_dir);
        state_read_end();

        if (can_play)
        {
            ssize_t wres = write(1, &best_dir, 1);
            (void)wres;
        }
        else
        {
            send_pass_and_wait(G, my);
            break;
        }
    }

    return 0;
}
