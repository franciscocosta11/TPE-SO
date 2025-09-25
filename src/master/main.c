// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
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
#include "master_logic.h"
#include "rules.h"
#include "shm.h"

/* --- señales --- */
static volatile sig_atomic_t stop_flag = 0;
static void on_signal(int sig)
{
    (void)sig;
    stop_flag = 1;
}

/* --- helpers de tiempo --- */
static long ms_since(const struct timespec *t0)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long ms = (now.tv_sec - t0->tv_sec) * 1000L + (now.tv_nsec - t0->tv_nsec) / 1000000L; // MAGIC NUMBER
    return ms;
}

/* sleep en milisegundos */
static void msleep_int(int ms)
{
    if (ms <= 0)
        return;
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

/* marcar un descriptor como close-on-exec para que no lo hereden futuros exec */
static void set_cloexec(int fd)
{
    if (fd < 0) return;
    int flags = fcntl(fd, F_GETFD);
    if (flags == -1) return;
    (void)fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

/* cerrar todos los FDs >=3 para que el hijo no herede nada extra */
static void close_fds_except_stdio(void)
{
    long maxfd = sysconf(_SC_OPEN_MAX);
    if (maxfd < 0)
        maxfd = 256; /* fallback razonable */
    for (int fd = 3; fd < (int)maxfd; ++fd)
        close(fd);
}

// ranking final
static void print_ranking(const GameState *G)
{
    /* copiar índices [0..n-1] y ordenar por score desc */
    unsigned n = G->n_players;
    unsigned idxs[MAX_PLAYERS];
    for (unsigned i = 0; i < n; ++i)
        idxs[i] = i;

    /* bubble simple (n<=9) */
    for (unsigned a = 0; a + 1 < n; ++a)
    {
        for (unsigned b = a + 1; b < n; ++b)
        {
            if (G->P[idxs[b]].score > G->P[idxs[a]].score)
            {
                unsigned aux = idxs[a];
                idxs[a] = idxs[b];
                idxs[b] = aux;
            }
        }
    }

    printf("\n=== FINAL RANKING ===\n");
    for (unsigned k = 0; k < n; ++k)
    {
        unsigned i = idxs[k];
        const Player *p = &G->P[i];
        printf("#%u  P%c  score=%u  valids=%u  invalids=%u  timeouts=%u  pos=(%u,%u)%s\n",
               k + 1, 'A' + i, p->score, p->valids, p->invalids, p->timeouts,
               (unsigned)p->x, (unsigned)p->y, p->blocked ? " [BLOCKED]" : "");
    }
    printf("=====================\n");
}

static int spawn_player(const char *path, int pipefd[2], unsigned W, unsigned H)
{
    if (pipe(pipefd) == -1)
    {
        perror("pipe");
        exit(1);
    }
    /* Evitar herencia accidental en exec si algo queda abierto */
    set_cloexec(pipefd[0]);
    set_cloexec(pipefd[1]);
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        exit(1);
    }
    if (pid == 0)
    {
        /* Mantener stdout hacia el pipe */
        if (dup2(pipefd[1], 1) == -1)
        {
            perror("dup2");
            _exit(1);
        }
        /* Redirigir stderr del player a un log por PID para diagnóstico. */
        char logpath[256];
        pid_t mypid = getpid();
        snprintf(logpath, sizeof(logpath), "./logs/player-%d.log", (int)mypid);
        int lf = open(logpath, O_CREAT | O_WRONLY | O_APPEND
#ifdef O_CLOEXEC
                                   | O_CLOEXEC
#endif
                                   , 0644);
        if (lf != -1)
        {
            (void)dup2(lf, 2);
            close(lf);
        }
        close(pipefd[0]);
        close(pipefd[1]);
        /* Asegurar que no queden FDs heredados antes del exec */
        close_fds_except_stdio();
        char wbuf[16], hbuf[16];
        snprintf(wbuf, sizeof(wbuf), "%u", W);
        snprintf(hbuf, sizeof(hbuf), "%u", H);
        execl(path, path, wbuf, hbuf, (char *)NULL);
        perror("execl");
        _exit(127);
    }
    close(pipefd[1]);
    return pid;
}

static pid_t spawn_view(const char *path, unsigned W, unsigned H)
{
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        exit(1);
    }
    if (pid == 0)
    {
        /* Redirigir stdout y stderr de la view a un log por PID. */
        char logpath[256];
        pid_t mypid = getpid();
        snprintf(logpath, sizeof(logpath), "./logs/view-%d.log", (int)mypid);
        int lf = open(logpath, O_CREAT | O_WRONLY | O_APPEND
#ifdef O_CLOEXEC
                                   | O_CLOEXEC
#endif
                                   , 0644);
        if (lf != -1)
        {
            (void)dup2(lf, 2);
            close(lf);
        }
        char wbuf[16], hbuf[16];
        snprintf(wbuf, sizeof(wbuf), "%u", W);
        snprintf(hbuf, sizeof(hbuf), "%u", H);
        execl(path, path, wbuf, hbuf, (char *)NULL);
        perror("execl");
        _exit(127);
    }
    return pid;
}

int main(int argc, char *argv[])
{
    // tests de valgrind 
    (void)mkdir("./logs", 0755);

    // señales
    struct sigaction sa = {0};
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* parseo */
    MasterConfig cfg;
    if (parse_args(argc, argv, &cfg) != 0)
    {
        fprintf(stderr, "parse_args failed\n");
        return 1;
    }

    unsigned W = (unsigned)cfg.width;
    unsigned H = (unsigned)cfg.height;
    unsigned N = (unsigned)cfg.player_count;
    if (N == 0)
    {
        fprintf(stderr, "no players specified\n");
        return 1;
    }
    if (N > MAX_PLAYERS)
    {
        fprintf(stderr, "too many players: %u (max %d)\n", N, MAX_PLAYERS);
        return 1;
    }

    const char *default_player_path = "./player";

    /* SHMs */
    GameState *G = (GameState *)state_create(W, H);
    if (!G)
    {
        fprintf(stderr, "shm_create_map(/game_state) failed\n");
        exit(1);
    }

    if (sync_create() != 0)
    {
        fprintf(stderr, "sync_create failed\n");
        exit(1);
    }

    /* estado inicial */
    state_write_begin();
    state_zero(G, W, H, N);
    board_fill_rewards(G, cfg.seed);
    players_place_grid(G);
    state_write_end();

    /* bloqueados iniciales */
    int blocked[MAX_PLAYERS] = {0};
    state_write_begin();
    for (unsigned i = 0; i < N; ++i)
    {
        blocked[i] = !player_can_move(G, (int)i);
        G->P[i].blocked = blocked[i];
    }
    state_write_end();

    /* spawn view */
    pid_t view_pid = -1;
    if (cfg.view_path && cfg.view_path[0] != '\0')
    {
        view_pid = spawn_view(cfg.view_path, W, H);
    }
    int has_view = (view_pid > 0);

    /* spawn players */
    int rfd[MAX_PLAYERS];
    pid_t pids[MAX_PLAYERS];
    int alive[MAX_PLAYERS];
    int plogfd[MAX_PLAYERS];

    for (unsigned i = 0; i < N; ++i)
    {
        int pf[2];
        const char *pp = (cfg.player_paths[i] && cfg.player_paths[i][0]) ? cfg.player_paths[i]
                                                                         : default_player_path;
        pids[i] = spawn_player(pp, pf, W, H);
        rfd[i] = pf[0];
        set_cloexec(rfd[i]);
        alive[i] = 1;
        /* abrir descriptor para que el master pueda anotar los bytes recibidos en el log del player */
        char logpath[256];
        snprintf(logpath, sizeof(logpath), "./logs/player-%d.log", (int)pids[i]);
        int lf = open(logpath, O_CREAT | O_WRONLY | O_APPEND
#ifdef O_CLOEXEC
                                   | O_CLOEXEC
#endif
                                   , 0644);
        if (lf == -1)
        {
            fprintf(stderr, "master: open('%s') failed: %s\n", logpath, strerror(errno));
            plogfd[i] = -1;
        }
        else
        {
            plogfd[i] = lf;
            dprintf(plogfd[i], "MASTER: opened log for pid=%d\n", (int)pids[i]);
        }
    }

    /* PIDs en el estado */
    state_write_begin();
    for (unsigned i = 0; i < N; ++i)
        G->P[i].pid = pids[i];
    state_write_end();

    /* frame inicial (si hay vista) */
    if (has_view)
        view_signal_update_ready();

    int rounds = 0;
    const int MAX_ROUNDS = 200;

    /* timeout entre válidas: arrancar el reloj ahora */
    struct timespec last_valid_ts;
    clock_gettime(CLOCK_MONOTONIC, &last_valid_ts);

    /* índice de inicio para round-robin rotatorio */
    unsigned rr_start = 0;

    while (!stop_flag)
    {
        /* vivos? */
        int any_alive = 0;
        for (unsigned i = 0; i < N; ++i)
            if (alive[i])
            {
                any_alive = 1;
                break;
            }
        if (!any_alive)
            break;

        /* Re-sincronizar array local blocked[] con el estado compartido */
        state_read_begin();
        for (unsigned i = 0; i < N; ++i)
        {
            if (alive[i])
                blocked[i] = G->P[i].blocked;
        }
        state_read_end();

        /* all blocked? (única condición de bloqueo colectivo válida) */
        int all_blocked = 1;
        for (unsigned i = 0; i < N; ++i)
            if (alive[i] && !blocked[i])
            {
                all_blocked = 0;
                break;
            }
        if (all_blocked)
        {
            printf("termination: all alive players blocked\n");
            break;
        }

        /* timeouts de control */
        int valid_timeout_ms = (cfg.timeout > 0) ? cfg.timeout : 0;
        int player_timeout_ms = (cfg.player_timeout_ms > 0) ? cfg.player_timeout_ms : 0;

        for (unsigned k = 0; k < N && !stop_flag; ++k)
        {
            unsigned i = (rr_start + k) % N;
            if (!alive[i] || blocked[i])
                continue;

            /* chequeo de timeout global entre válidas */
            if (valid_timeout_ms > 0)
            {
                long since_valid = ms_since(&last_valid_ts);
                if (since_valid >= valid_timeout_ms)
                {
                    printf("termination: timeout between valid moves (%ld ms)\n", since_valid);
                    stop_flag = 1;
                    break;
                }
            }

            /* otorgar turno al jugador i */
            player_signal_turn((int)i);

            /* esperar movimiento del jugador i en su pipe con timeout individual */
            int remaining_ms = player_timeout_ms;
            int got_event = 0; /* 1 si recibimos algo (movimiento/EOF/error) */
            while (!got_event && !stop_flag)
            {
                /* chequear timeout global entre válidas durante la espera */
                if (valid_timeout_ms > 0)
                {
                    long since_valid = ms_since(&last_valid_ts);
                    if (since_valid >= valid_timeout_ms)
                    {
                        printf("termination: timeout between valid moves (%ld ms)\n", since_valid);
                        stop_flag = 1;
                        break;
                    }
                }

                fd_set rfds;
                FD_ZERO(&rfds);
                FD_SET(rfd[i], &rfds);
                struct timeval tv;
                if (remaining_ms > 0)
                {
                    tv.tv_sec = remaining_ms / 1000;
                    tv.tv_usec = (remaining_ms % 1000) * 1000;
                }
                else
                {
                    tv.tv_sec = 0;
                    tv.tv_usec = 0;
                }
                struct timespec t0;
                clock_gettime(CLOCK_MONOTONIC, &t0);
                int rv = select(rfd[i] + 1, &rfds, NULL, NULL, (player_timeout_ms > 0 ? &tv : NULL));
                if (rv < 0)
                {
                    if (errno == EINTR)
                        continue;
                    perror("select");
                    got_event = 1; /* tratamos como evento para avanzar */
                    alive[i] = 0;
                    close(rfd[i]);
                    break;
                }
                if (rv == 0)
                {
                    if (player_timeout_ms <= 0)
                        continue; /* sin timeout: seguimos esperando */
                    /* timeout individual: contabilizamos y seguimos con el siguiente jugador */
                    state_write_begin();
                    G->P[i].timeouts += 1;
                    state_write_end();
                    if (plogfd[i] != -1)
                        dprintf(plogfd[i], "TIMEOUT\n");
                    if (has_view)
                    {
                        view_signal_update_ready();
                        view_wait_render_complete();
                    }
                    msleep_int(cfg.delay);
                    break;
                }

                /* hay datos listos */
                uint8_t mv;
                ssize_t n = read(rfd[i], &mv, 1);
                if (n == 1)
                {
                    got_event = 1;
                    int gain = 0;
                    if (plogfd[i] != -1)
                        dprintf(plogfd[i], "mv=%u\n", (unsigned)mv);

                    state_write_begin();
                    if (mv == 0xFF)
                    {
                        blocked[i] = 1;
                        G->P[i].blocked = 1;
                        printf("[round %d] player %u PASS -> BLOCKED\n", rounds, i);
                    }
                    else
                    {
                        int ok = (mv < 8) && rules_validate(G, (int)i, (Dir)mv, &gain);
                        if (ok)
                        {
                            rules_apply(G, (int)i, (Dir)mv);
                            printf("[round %d] player %u VALID dir=%u gain=%d score=%u pos=(%u,%u)\n",
                                   rounds, i, (unsigned)mv, gain,
                                   G->P[i].score, (unsigned)G->P[i].x, (unsigned)G->P[i].y);
                            clock_gettime(CLOCK_MONOTONIC, &last_valid_ts);
                        }
                        else
                        {
                            G->P[i].invalids++;
                            printf("[round %d] player %u INVALID dir=%u (invalids=%u)\n",
                                   rounds, i, (unsigned)mv, G->P[i].invalids);
                        }
                        blocked[i] = !player_can_move(G, (int)i);
                        G->P[i].blocked = blocked[i];
                        if (blocked[i])
                            printf("player %u BLOCKED (no moves)\n", i);
                    }
                    state_write_end();

                    if (has_view)
                    {
                        view_signal_update_ready();
                        view_wait_render_complete();
                    }
                    msleep_int(cfg.delay);
                }
                else if (n == 0)
                {
                    got_event = 1;
                    alive[i] = 0;
                    close(rfd[i]);
                    if (plogfd[i] != -1)
                        dprintf(plogfd[i], "EOF\n");
                    printf("player %u EOF\n", i);
                    if (has_view)
                    {
                        view_signal_update_ready();
                        view_wait_render_complete();
                    }
                    msleep_int(cfg.delay);
                }
                else
                {
                    if (errno == EINTR)
                        continue;
                    if (errno != EAGAIN && errno != EWOULDBLOCK)
                    {
                        got_event = 1;
                        perror("read");
                        alive[i] = 0;
                        close(rfd[i]);
                        if (plogfd[i] != -1)
                            dprintf(plogfd[i], "ERROR read errno=%d\n", errno);
                        if (has_view)
                        {
                            view_signal_update_ready();
                            view_wait_render_complete();
                        }
                        msleep_int(cfg.delay);
                    }
                }

                if (player_timeout_ms > 0)
                {
                    struct timespec t1;
                    clock_gettime(CLOCK_MONOTONIC, &t1);
                    long spent = (t1.tv_sec - t0.tv_sec) * 1000L + (t1.tv_nsec - t0.tv_nsec) / 1000000L;
                    if (spent > 0 && remaining_ms > 0)
                        remaining_ms = remaining_ms > (int)spent ? remaining_ms - (int)spent : 0;
                }
            }
        }

        if (has_view)
        {
            view_signal_update_ready();
            view_wait_render_complete();
        }

        int all_blocked2 = 1;
        for (unsigned i = 0; i < N; ++i)
            if (alive[i] && !blocked[i])
            {
                all_blocked2 = 0;
                break;
            }
        if (all_blocked2)
        {
            printf("termination: all alive players blocked (post-round)\n");
            break;
        }

        rounds++;
        if (rounds >= MAX_ROUNDS)
        {
            printf("max rounds reached\n");
            break;
        }
    }

    /* fin del juego */
    state_write_begin();
    G->game_over = true;
    state_write_end();

    if (has_view)
        view_signal_update_ready();
    // no esperamos render

    for (unsigned i = 0; i < N; ++i)
        if (pids[i] > 0)
            kill(pids[i], SIGTERM);
    for (unsigned i = 0; i < N; ++i)
        if (pids[i] > 0)
        {
            int status = 0;
            (void)waitpid(pids[i], &status, 0);
            int exited = WIFEXITED(status);
            int code = exited ? WEXITSTATUS(status) : -1;
            int signaled = WIFSIGNALED(status);
            int sig = signaled ? WTERMSIG(status) : 0;
            /* imprimir causa y puntaje */
            unsigned score = 0;
            state_read_begin();
            if (i < G->n_players)
                score = G->P[i].score;
            state_read_end();
            if (exited)
                printf("player %u exited code=%d score=%u\n", i, code, score);
            else if (signaled)
                printf("player %u signaled sig=%d score=%u\n", i, sig, score);
            else
                printf("player %u done (unknown status) score=%u\n", i, score);
        }
    if (view_pid > 0)
    {
        int status = 0;
        (void)waitpid(view_pid, &status, 0);
        if (WIFEXITED(status))
            printf("view exited code=%d\n", WEXITSTATUS(status));
        else if (WIFSIGNALED(status))
            printf("view signaled sig=%d\n", WTERMSIG(status));
    }

    /* cerrar logs abiertos por el master */
    for (unsigned i = 0; i < N; ++i)
        if (plogfd[i] != -1)
            close(plogfd[i]);

    printf("done after %d rounds\n", rounds);

    /* ranking final (Día 6) */
    state_read_begin();
    print_ranking(G);
    state_read_end();

    state_destroy(G);
    sync_destroy();
    /* limpiar segmento shm remanente si el módulo de estado no lo desvincula */
    (void)shm_remove_name(SHM_GAME_STATE);
    return 0;
}
