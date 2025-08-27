// src/master/main.c
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

#include "state.h"
#include "sync.h"

#define MAXP 9

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

int main(void) {
    /* --- parámetros base --- */
    unsigned W = 12, H = 8, N = 3;
    int n_players = (int)N;
    const char *player_path = "./player";

    if (n_players > MAXP) n_players = MAXP;

    /* --- /game_state en shm --- */
    int gfd = shm_open(SHM_GAME_STATE, O_CREAT | O_RDWR, 0600);
    if (gfd == -1) { perror("shm_open(/game_state)"); exit(1); }
    size_t GSIZE = state_size(W, H);
    if (ftruncate(gfd, (off_t)GSIZE) == -1) { perror("ftruncate"); exit(1); }
    GameState *G = mmap(NULL, GSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, gfd, 0);
    if (G == MAP_FAILED) { perror("mmap(/game_state)"); exit(1); }

    /* --- /game_sync para R/W locks --- */
    if (sync_create() != 0) { fprintf(stderr, "sync_create failed\n"); exit(1); }

    /* --- estado inicial bajo lock de escritura --- */
    wrlock();
    state_zero(G, W, H, N);
    board_fill_rewards(G, 42);
    players_place_grid(G);
    wrunlock();

    /* blocked[] local (para la lógica del master) y reflejado en G->P[].blocked */
    int blocked[MAXP] = {0};
    wrlock();
    for (int i = 0; i < (int)N; ++i) {
        blocked[i] = !player_can_move(G, i);
        G->P[i].blocked = blocked[i];
    }
    wrunlock();

    /* --- spawn jugadores (pipes stdout->master) --- */
    int   rfd[MAXP];      // read-ends
    pid_t pids[MAXP];
    int   alive[MAXP];    // 1 si el pipe sigue abierto
    int   took_turn[MAXP];// 1 si ya consumió su 1 byte en la ronda

    for (int i = 0; i < n_players; ++i) {
        int pf[2];
        pids[i] = spawn_player(player_path, pf);
        rfd[i] = pf[0];
        alive[i] = 1;
        took_turn[i] = 0;
    }

    int rounds = 0;
    const int MAX_ROUNDS = 200;  // límite de seguridad por si algo queda colgado

    while (1) {
        /* ¿Quedan jugadores vivos? (FD abierto) */
        int any_alive = 0;
        for (int i = 0; i < n_players; ++i) if (alive[i]) { any_alive = 1; break; }
        if (!any_alive) break;

        /* ¿Todos los vivos están bloqueados? -> terminar */
        int all_blocked = 1;
        for (int i = 0; i < n_players; ++i) {
            if (alive[i] && !blocked[i]) { all_blocked = 0; break; }
        }
        if (all_blocked) {
            printf("all players blocked\n");
            break;
        }

        /* ¿Terminé la ronda? (todos los vivos y NO bloqueados ya mandaron 1 byte) */
        int round_done = 1;
        for (int i = 0; i < n_players; ++i) {
            if (alive[i] && !blocked[i] && !took_turn[i]) { round_done = 0; break; }
        }
        if (round_done) {
            memset(took_turn, 0, sizeof(took_turn));
            rounds++;
            if (rounds >= MAX_ROUNDS) { printf("max rounds reached\n"); break; }
        }

        /* Armado de fd_set solo con vivos y no bloqueados que no hayan jugado en la ronda */
        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = -1;
        for (int i = 0; i < n_players; ++i) {
            if (alive[i] && !blocked[i] && !took_turn[i]) {
                FD_SET(rfd[i], &rfds);
                if (rfd[i] > maxfd) maxfd = rfd[i];
            }
        }
        if (maxfd < 0) {
            // Nada pendiente pero quizá recién cerraron/EOF; sigue a recolectar hijos
            goto collect_children;
        }

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 300000; // 300 ms
        int rv = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        if (rv == 0) {
            goto collect_children;
        }
        if (rv < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        /* Procesar a los listos: 1 byte por jugador en esta ronda */
        for (int i = 0; i < n_players; ++i) {
            if (!alive[i] || took_turn[i] || blocked[i]) continue;
            if (!FD_ISSET(rfd[i], &rfds)) continue;

            uint8_t mv;
            ssize_t n = read(rfd[i], &mv, 1);
            if (n == 1) {
                took_turn[i] = 1;
                int gain = 0;

                /* Validar y aplicar bajo lock de escritura (modifica G) */
                wrlock();
                int ok = (mv < 8) && rules_validate(G, i, (Dir)mv, &gain);
                if (ok) {
                    rules_apply(G, i, (Dir)mv);
                    printf("[round %d] player %d VALID dir=%u gain=%d score=%u pos=(%u,%u)\n",
                           rounds, i, (unsigned)mv, gain,
                           G->P[i].score, G->P[i].x, G->P[i].y);
                } else {
                    G->P[i].invalids++;
                    printf("[round %d] player %d INVALID dir=%u (invalids=%u)\n",
                           rounds, i, (unsigned)mv, G->P[i].invalids);
                }

                /* Recalcular bloqueo y reflejar en estado */
                blocked[i] = !player_can_move(G, i);
                G->P[i].blocked = blocked[i];
                if (blocked[i]) {
                    printf("player %d BLOCKED (no moves)\n", i);
                }
                wrunlock();

                fflush(stdout);
            } else if (n == 0) {
                // EOF del jugador
                alive[i] = 0;
                close(rfd[i]);
                printf("player %d EOF\n", i);
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // nada; volverá a estar listo
                } else {
                    perror("read");
                    alive[i] = 0;
                    close(rfd[i]);
                }
            }
        }

    collect_children:
        /* Recolectar hijos terminados (evita zombies) */
        for (int i = 0; i < n_players; ++i) {
            if (!alive[i]) {
                int status;
                (void)waitpid(pids[i], &status, WNOHANG);
            }
        }
    }

    /* Señalar fin de juego en el estado compartido */
    wrlock();
    G->game_over = true;
    for (int i = 0; i < n_players; ++i) {
        if (pids[i] > 0) kill(pids[i], SIGTERM); // <signal.h>
    }
    for (int i = 0; i < n_players; ++i) {
        if (pids[i] > 0) waitpid(pids[i], NULL, 0);
    }
    wrunlock();

    printf("done after %d rounds\n", rounds);

    /* Limpieza */
    munmap(G, GSIZE);
    close(gfd);
    shm_unlink(SHM_GAME_STATE);
    sync_destroy();
    return 0;
}
