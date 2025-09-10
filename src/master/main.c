#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>

#include "state.h"
#include "state_access.h"
#include "sync.h"
#include "shm.h"
#include "master_logic.h"

#define MAXP 9

/* --- señales --- */
static volatile sig_atomic_t stop_flag = 0;
static void on_signal(int sig) { (void)sig; stop_flag = 1; }

/* --- helpers de tiempo --- */
static long ms_since(const struct timespec *t0) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long ms = (now.tv_sec - t0->tv_sec) * 1000L + (now.tv_nsec - t0->tv_nsec) / 1000000L;
    return ms;
}

/* --- ranking final --- */
static void print_ranking(const GameState *G) {
    /* copiar índices [0..n-1] y ordenar por score desc */
    unsigned n = G->n_players;
    unsigned idxs[MAXP];
    for (unsigned i = 0; i < n; ++i) idxs[i] = i;

    /* bubble simple (n<=9) */
    for (unsigned a = 0; a + 1 < n; ++a) {
        for (unsigned b = a + 1; b < n; ++b) {
            if (G->P[idxs[b]].score > G->P[idxs[a]].score) {
                unsigned tmp = idxs[a]; idxs[a] = idxs[b]; idxs[b] = tmp;
            }
        }
    }

    printf("\n=== FINAL RANKING ===\n");
    for (unsigned k = 0; k < n; ++k) {
        unsigned i = idxs[k];
        const Player *p = &G->P[i];
        printf("#%u  P%u  score=%u  valids=%u  invalids=%u  timeouts=%u  pos=(%u,%u)%s\n",
               k+1, i, p->score, p->valids, p->invalids, p->timeouts,
               (unsigned)p->x, (unsigned)p->y, p->blocked ? " [BLOCKED]" : "");
    }
    printf("=====================\n");
}

/* --- spawners --- */
static int spawn_player(const char *path, int pipefd[2]) {
    if (pipe(pipefd) == -1) { perror("pipe"); exit(1); }
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(1); }
    if (pid == 0) {
        if (dup2(pipefd[1], 1) == -1) { perror("dup2"); _exit(1); }
        close(pipefd[0]);
        close(pipefd[1]);
        execl(path, path, (char*)NULL);
        perror("execl"); _exit(127);
    }
    close(pipefd[1]);
    return pid;
}

static pid_t spawn_view(const char *path) {
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(1); }
    if (pid == 0) {
        execl(path, path, (char*)NULL);
        perror("execl"); _exit(127);
    }
    return pid;
}

int main(int argc, char *argv[]) {
    /* señales */
    struct sigaction sa = {0};
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* parseo */
    MasterConfig cfg;
    if (parse_args(argc, argv, &cfg) != 0) {
        fprintf(stderr, "parse_args failed\n");
        return 1;
    }

    unsigned W = (unsigned)cfg.width;
    unsigned H = (unsigned)cfg.height;
    unsigned N = (unsigned)cfg.player_count;
    if (N == 0) { fprintf(stderr, "no players specified\n"); return 1; }
    if (N > MAXP) N = MAXP;

    const char *default_player_path = "./player";

    /* SHMs */
    size_t GSIZE = state_size(W, H);
    GameState *G = (GameState*)shm_create_map(SHM_GAME_STATE, GSIZE, PROT_READ | PROT_WRITE);
    if (!G) { fprintf(stderr, "shm_create_map(/game_state) failed\n"); exit(1); }

    if (sync_create() != 0) { fprintf(stderr, "sync_create failed\n"); exit(1); }

    /* estado inicial */
    state_write_begin();
    state_zero(G, W, H, N);
    board_fill_rewards(G, cfg.seed);
    players_place_grid(G);
    state_write_end();

    /* bloqueados iniciales */
    int blocked[MAXP] = {0};
    state_write_begin();
    for (unsigned i = 0; i < N; ++i) {
        blocked[i] = !player_can_move(G, (int)i);
        G->P[i].blocked = blocked[i];
    }
    state_write_end();

    /* spawn view */
    pid_t view_pid = -1;
    if (cfg.view_path && cfg.view_path[0] != '\0') {
        view_pid = spawn_view(cfg.view_path);
    }

    /* spawn players */
    int   rfd[MAXP];
    pid_t pids[MAXP];
    int   alive[MAXP];
    int   took_turn[MAXP];

    for (unsigned i = 0; i < N; ++i) {
        int pf[2];
        const char *pp = (cfg.player_paths[i] && cfg.player_paths[i][0]) ? cfg.player_paths[i]
                                                                         : default_player_path;
        pids[i] = spawn_player(pp, pf);
        rfd[i] = pf[0];
        alive[i] = 1;
        took_turn[i] = 0;
    }

    /* PIDs en el estado */
    state_write_begin();
    for (unsigned i = 0; i < N; ++i) G->P[i].pid = pids[i];
    state_write_end();

    /* frame inicial */
    view_signal_update_ready();

    /* habilitar 1er turno */
    for (unsigned i = 0; i < N; ++i) if (alive[i] && !blocked[i]) player_signal_turn((int)i);

    int rounds = 0;
    const int MAX_ROUNDS = 200;

    while (!stop_flag) {
        /* vivos? */
        int any_alive = 0;
        for (unsigned i = 0; i < N; ++i) if (alive[i]) { any_alive = 1; break; }
        if (!any_alive) break;

        /* Re-sincronizar array local blocked[] con el estado compartido para evitar drift */
        state_read_begin();
        for (unsigned i = 0; i < N; ++i) {
            if (alive[i]) blocked[i] = G->P[i].blocked;
        }
        state_read_end();

    /* all blocked? (única condición de bloqueo colectivo válida) */
    int all_blocked = 1;
    for (unsigned i = 0; i < N; ++i) if (alive[i] && !blocked[i]) { all_blocked = 0; break; }
    if (all_blocked) { printf("termination: all alive players blocked\n"); break; }

        /* tiempos de control */
        struct timespec t_round0;
        clock_gettime(CLOCK_MONOTONIC, &t_round0);
        int round_timeout_ms   = (cfg.timeout > 0) ? cfg.timeout : 0;
        int player_timeout_ms  = (cfg.player_timeout_ms > 0) ? cfg.player_timeout_ms : 0;

    /* loop interno de la ronda */
    int progress_this_round = 0; /* 1 si hubo movimiento / EOF / timeout aplicado */
    while (1) {
            /* fin por completitud */
            int round_done = 1;
            for (unsigned i = 0; i < N; ++i)
                if (alive[i] && !blocked[i] && !took_turn[i]) { round_done = 0; break; }
            if (round_done) break;

            /* aplicar timeouts individuales (Día 6) */
            if (player_timeout_ms > 0) {
                long elapsed_ms = ms_since(&t_round0);
                if (elapsed_ms >= player_timeout_ms) {
                    state_write_begin();
                    for (unsigned i = 0; i < N; ++i) {
                        if (alive[i] && !blocked[i] && !took_turn[i]) {
                            took_turn[i] = 1;
                            G->P[i].timeouts += 1; /* cuenta timeout individual */
                            progress_this_round = 1;
                            /* no modificamos blocked: podrá jugar próxima ronda si tiene jugadas */
                        }
                    }
                    state_write_end();
                }
            }

            /* fin por timeout de ronda (si aplica) */
            if (round_timeout_ms > 0) {
                long elapsed_ms = ms_since(&t_round0);
                if (elapsed_ms >= round_timeout_ms) {
                    /* se cierra la ronda: los que no jugaron “gastan turno” */
                    state_write_begin();
                    for (unsigned i = 0; i < N; ++i) {
                        if (alive[i] && !blocked[i] && !took_turn[i]) {
                            took_turn[i] = 1;
                            progress_this_round = 1;
                            /* Podríamos también sumar timeout aquí si querés; lo dejamos solo para per-jugador */
                        }
                    }
                    state_write_end();
                    break;
                }
            }

            /* esperar pipes listos */
            fd_set rfds; FD_ZERO(&rfds);
            int maxfd = -1;
            for (unsigned i = 0; i < N; ++i) {
                if (alive[i] && !blocked[i] && !took_turn[i]) {
                    FD_SET(rfd[i], &rfds);
                    if (rfd[i] > maxfd) maxfd = rfd[i];
                }
            }
            if (maxfd < 0) break;

            struct timeval tv;
            if (cfg.delay > 0) {
                tv.tv_sec  = (cfg.delay >= 1000) ? (cfg.delay / 1000) : 0;
                tv.tv_usec = (cfg.delay % 1000) * 1000;
            } else { tv.tv_sec = 0; tv.tv_usec = 10000; }

            int rv = select(maxfd + 1, &rfds, NULL, NULL, &tv);
            if (rv < 0) { if (errno == EINTR) continue; perror("select"); break; }
            if (rv == 0) { /* poll timeout */ goto collect_children_inner; }

            for (unsigned i = 0; i < N; ++i) {
                if (!alive[i] || took_turn[i] || blocked[i]) continue;
                if (!FD_ISSET(rfd[i], &rfds)) continue;

                uint8_t mv;
                ssize_t n = read(rfd[i], &mv, 1);
                if (n == 1) {
                    took_turn[i] = 1;
                    int gain = 0;

                    state_write_begin();
                    if (mv == 0xFF) { /* PASS sentinel: jugador sin movimientos disponibles */
                        blocked[i] = 1;
                        G->P[i].blocked = 1;
                        printf("[round %d] player %u PASS -> BLOCKED\n", rounds, i);
                    } else {
                        int ok = (mv < 8) && rules_validate(G, (int)i, (Dir)mv, &gain);
                        if (ok) {
                            rules_apply(G, (int)i, (Dir)mv);
                            printf("[round %d] player %u VALID dir=%u gain=%d score=%u pos=(%u,%u)\n",
                                   rounds, i, (unsigned)mv, gain,
                                   G->P[i].score, (unsigned)G->P[i].x, (unsigned)G->P[i].y);
                        } else {
                            G->P[i].invalids++;
                            printf("[round %d] player %u INVALID dir=%u (invalids=%u)\n",
                                   rounds, i, (unsigned)mv, G->P[i].invalids);
                        }

                        blocked[i] = !player_can_move(G, (int)i);
                        G->P[i].blocked = blocked[i];
                        if (blocked[i]) printf("player %u BLOCKED (no moves)\n", i);
                    }
                    state_write_end();
                    progress_this_round = 1;
                } else if (n == 0) {
                    alive[i] = 0;
                    close(rfd[i]);
                    printf("player %u EOF\n", i);
                    progress_this_round = 1;
                } else {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("read");
                        alive[i] = 0;
                        close(rfd[i]);
                        progress_this_round = 1;
                    }
                }
            }

        collect_children_inner:
            for (unsigned i = 0; i < N; ++i) {
                if (!alive[i]) {
                    int status;
                    (void)waitpid(pids[i], &status, WNOHANG);
                }
            }
            /* watchdog de ronda: si pasan 5000ms sin progreso forzamos cierre */
            if (!progress_this_round) {
                long elapsed_ms2 = ms_since(&t_round0);
                if (elapsed_ms2 > 5000) {
                    fprintf(stderr, "watchdog: forcing end of round after %ld ms without progress\n", elapsed_ms2);
                    break;
                }
            }
        } /* fin loop interno */

        /* view: fin de ronda */
        view_signal_update_ready();
        view_wait_render_complete();

        /* preparar próxima ronda */
        memset(took_turn, 0, sizeof(took_turn));

    int all_blocked2 = 1;
    for (unsigned i = 0; i < N; ++i) if (alive[i] && !blocked[i]) { all_blocked2 = 0; break; }
    if (all_blocked2) { printf("termination: all alive players blocked (post-round)\n"); break; }

        rounds++;
        if (rounds >= MAX_ROUNDS) { printf("max rounds reached\n"); break; }

        for (unsigned i = 0; i < N; ++i) if (alive[i] && !blocked[i]) player_signal_turn((int)i);
    }

    /* fin del juego */
    state_write_begin();
    G->game_over = true;
    state_write_end();

    view_signal_update_ready();
    // view_wait_render_complete();

    for (unsigned i = 0; i < N; ++i) if (pids[i] > 0) kill(pids[i], SIGTERM);
    for (unsigned i = 0; i < N; ++i) if (pids[i] > 0) (void)waitpid(pids[i], NULL, 0);
    if (view_pid > 0) { kill(view_pid, SIGTERM); (void)waitpid(view_pid, NULL, 0); }

    printf("done after %d rounds\n", rounds);

    /* ranking final (Día 6) */
    state_read_begin();
    print_ranking(G);
    state_read_end();

    munmap(G, GSIZE);
    shm_unlink(SHM_GAME_STATE);
    sync_destroy();
    return 0;
}
