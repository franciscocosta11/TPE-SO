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


#include "state.h"
#include "state_access.h"
#include "sync.h"
#include "shm.h"
#include "master_logic.h"


#define MAXP 9

/* definiciones ya declaradas arriba; se eliminan duplicados */

static int spawn_player(const char *path, int pipefd[2]) {
    if (pipe(pipefd) == -1) { perror("pipe"); exit(1); }
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(1); }
    if (pid == 0) {
        // Hijo: stdout -> write-end del pipe
        if (dup2(pipefd[1], 1) == -1) { perror("dup2"); _exit(1); }
        close(pipefd[0]);
        close(pipefd[1]);
        execl(path, path, (char*)NULL);
        perror("execl"); _exit(127);
    }
    // Padre: solo read-end
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


static volatile sig_atomic_t stop_flag = 0;
static void on_signal(int sig) { (void)sig; stop_flag = 1; }

int main(int argc, char *argv[]) {
    /* --- señales para limpieza prolija --- */
    struct sigaction sa = {0};
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* --- parseo de parámetros --- */
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

    /* --- SHM: /game_state --- */
    size_t GSIZE = state_size(W, H);
    GameState *G = (GameState*)shm_create_map(SHM_GAME_STATE, GSIZE, PROT_READ | PROT_WRITE);
    if (!G) { fprintf(stderr, "shm_create_map(/game_state) failed\n"); exit(1); }

    /* --- SHM: /game_sync --- */
    if (sync_create() != 0) { fprintf(stderr, "sync_create failed\n"); exit(1); }

    /* --- estado inicial bajo lock de escritura --- */
    state_write_begin();
    state_zero(G, W, H, N);
    board_fill_rewards(G, cfg.seed);
    players_place_grid(G);
    state_write_end();

    /* --- bloqueo inicial por jugador --- */
    int blocked[MAXP] = {0};
    state_write_begin();
    for (unsigned i = 0; i < N; ++i) {
        blocked[i] = !player_can_move(G, (int)i);
        G->P[i].blocked = blocked[i];
    }
    state_write_end();

    /* --- spawn viewer opcional --- */
    pid_t view_pid = -1;
    if (cfg.view_path && cfg.view_path[0] != '\0') {
        view_pid = spawn_view(cfg.view_path);
    }

    /* --- spawn jugadores (pipes stdout->master) --- */
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

    /* --- guardar PIDs en el estado para que los players se identifiquen --- */
    state_write_begin();
    for (unsigned i = 0; i < N; ++i) {
        G->P[i].pid = pids[i];
    }
    state_write_end();

    /* --- frame inicial para la view --- */
    view_signal_update_ready();

    int rounds = 0;
    const int MAX_ROUNDS = 200;

    while (!stop_flag) {
        /* ¿Quedan jugadores vivos? */
        int any_alive = 0;
        for (unsigned i = 0; i < N; ++i) if (alive[i]) { any_alive = 1; break; }
        if (!any_alive) break;

        /* ¿Todos los vivos están bloqueados? */
        int all_blocked = 1;
        for (unsigned i = 0; i < N; ++i) {
            if (alive[i] && !blocked[i]) { all_blocked = 0; break; }
        }
        if (all_blocked) {
            printf("all players blocked\n");
            break;
        }

        /* ¿Se terminó la ronda? (todos los vivos y no bloqueados ya jugaron) */
        int round_done = 1;
        for (unsigned i = 0; i < N; ++i) {
            if (alive[i] && !blocked[i] && !took_turn[i]) { round_done = 0; break; }
        }
        if (round_done) {
            memset(took_turn, 0, sizeof(took_turn));
            rounds++;

            /* publicar fin de ronda para la view */
            view_signal_update_ready();
            /* opcional: hacerlo estrictamente sin tearing */
            // view_wait_render_complete();

            if (rounds >= MAX_ROUNDS) { printf("max rounds reached\n"); break; }
        }

        /* Armado de fd_set con vivos y habilitados que no hayan jugado */
        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = -1;
        for (unsigned i = 0; i < N; ++i) {
            if (alive[i] && !blocked[i] && !took_turn[i]) {
                FD_SET(rfd[i], &rfds);
                if (rfd[i] > maxfd) maxfd = rfd[i];
            }
        }
        if (maxfd < 0) goto collect_children;

        /* Delay de poll configurable -d (ms) */
        struct timeval tv;
        tv.tv_sec  = (cfg.delay >= 1000) ? (cfg.delay / 1000) : 0;
        tv.tv_usec = (cfg.delay % 1000) * 1000;

        int rv = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        if (rv == 0) { /* timeout: revisar si cerraron pipes */ goto collect_children; }
        if (rv < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        /* Procesar listos: 1 byte por jugador en esta ronda */
        for (unsigned i = 0; i < N; ++i) {
            if (!alive[i] || took_turn[i] || blocked[i]) continue;
            if (!FD_ISSET(rfd[i], &rfds)) continue;

            uint8_t mv;
            ssize_t n = read(rfd[i], &mv, 1);
            if (n == 1) {
                took_turn[i] = 1;
                int gain = 0;

                state_write_begin();
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
                if (blocked[i]) {
                    printf("player %u BLOCKED (no moves)\n", i);
                }
                state_write_end();

                fflush(stdout);
            } else if (n == 0) {
                /* EOF del jugador */
                alive[i] = 0;
                close(rfd[i]);
                printf("player %u EOF\n", i);
            } else {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    perror("read");
                    alive[i] = 0;
                    close(rfd[i]);
                }
            }
        }

    collect_children:
        for (unsigned i = 0; i < N; ++i) {
            if (!alive[i]) {
                int status;
                (void)waitpid(pids[i], &status, WNOHANG);
            }
        }
    }

    /* --- Fin de juego --- */
    state_write_begin();
    G->game_over = true;
    state_write_end();

    /* publicar frame final para la view */
    view_signal_update_ready();
    // view_wait_render_complete(); // opcional

    /* terminar players y view */
    for (unsigned i = 0; i < N; ++i) if (pids[i] > 0) kill(pids[i], SIGTERM);
    for (unsigned i = 0; i < N; ++i) if (pids[i] > 0) (void)waitpid(pids[i], NULL, 0);
    if (view_pid > 0) { kill(view_pid, SIGTERM); (void)waitpid(view_pid, NULL, 0); }

    printf("done after %d rounds\n", rounds);

    /* Limpieza SHM/SYNC */
    munmap(G, GSIZE);
    shm_unlink(SHM_GAME_STATE);
    sync_destroy();

    return 0;
}

