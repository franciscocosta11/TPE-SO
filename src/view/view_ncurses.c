// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
// view/ncurses_main.c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <ncurses.h>
#include <signal.h>
#include <stdbool.h>

#include "state.h"
#include "state_access.h"
#include "sync.h"

// Colores para los jugadores
#define COLOR_PLAYER_BASE 10
#define COLOR_PLAYER_CURRENT (COLOR_PLAYER_BASE + 20)
#define COLOR_REWARD 20
#define COLOR_UI 30
#define CELL_HEIGHT 3
#define CELL_WIDTH 5

// Globals para cleanup
static GameState *global_state = NULL;
static int ncurses_initialized = 0;
static SCREEN *global_scr = NULL;
static volatile sig_atomic_t g_should_exit = 0;
static int g_has_colors = 0;

static void request_exit(int sig)
{
    (void)sig;
    g_should_exit = 1; // solo marcar; el cleanup real se hace en el main loop
}

static void install_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = request_exit;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static void cleanup_and_exit(int code)
{
    if (ncurses_initialized)
    {
        // Restaurar terminal y liberar estructuras internas
        // endwin() primero, luego delscreen() si se usó newterm()
        endwin();
        if (global_scr)
        {
            delscreen(global_scr);
            global_scr = NULL;
        }
        ncurses_initialized = 0;
    }
    if (global_state)
    {
        state_destroy(global_state);
        global_state = NULL;
    }
    // Detach de primitivos de sync si los usamos
    _exit(code);
}

/* sin sleep: el master controla el ritmo */

static void init_colors(void)
{
    g_has_colors = has_colors();
    if (g_has_colors)
    {
        start_color();
        use_default_colors();

        // Players
        init_pair(COLOR_PLAYER_BASE + 0, COLOR_WHITE, COLOR_RED);
        init_pair(COLOR_PLAYER_BASE + 1, COLOR_WHITE, COLOR_GREEN);
        init_pair(COLOR_PLAYER_BASE + 2, COLOR_WHITE, COLOR_YELLOW);
        init_pair(COLOR_PLAYER_BASE + 3, COLOR_WHITE, COLOR_BLUE);
        init_pair(COLOR_PLAYER_BASE + 4, COLOR_WHITE, COLOR_MAGENTA);
        init_pair(COLOR_PLAYER_BASE + 5, COLOR_WHITE, COLOR_CYAN);
        init_pair(COLOR_PLAYER_BASE + 6, COLOR_WHITE, COLOR_RED);
        init_pair(COLOR_PLAYER_BASE + 7, COLOR_WHITE, COLOR_GREEN);
        // Colores especiales para la cabeza: letra del color del jugador sobre su propio fondo
        init_pair(COLOR_PLAYER_BASE + 20, COLOR_RED, COLOR_RED);         // cabeza rojo (A)
        init_pair(COLOR_PLAYER_BASE + 21, COLOR_GREEN, COLOR_GREEN);     // cabeza verde (B)
        init_pair(COLOR_PLAYER_BASE + 22, COLOR_YELLOW, COLOR_YELLOW);   // cabeza amarillo (C)
        init_pair(COLOR_PLAYER_BASE + 23, COLOR_BLUE, COLOR_BLUE);       // cabeza azul (D)
        init_pair(COLOR_PLAYER_BASE + 24, COLOR_MAGENTA, COLOR_MAGENTA); // cabeza magenta (E)
        init_pair(COLOR_PLAYER_BASE + 25, COLOR_CYAN, COLOR_CYAN);       // cabeza cian (F)
        init_pair(COLOR_PLAYER_BASE + 26, COLOR_RED, COLOR_RED);         // cabeza rojo (G)
        init_pair(COLOR_PLAYER_BASE + 27, COLOR_GREEN, COLOR_GREEN);     // cabeza verde (H)

        // UI
        init_pair(COLOR_UI + 0, COLOR_WHITE, COLOR_BLACK); // bordes en blanco
        init_pair(COLOR_UI + 1, COLOR_YELLOW, COLOR_BLACK);
        init_pair(COLOR_UI + 2, COLOR_RED, COLOR_BLACK);
        init_pair(COLOR_UI + 3, COLOR_GREEN, COLOR_BLACK);

        // Recompensas
        init_pair(COLOR_REWARD + 0, COLOR_WHITE, COLOR_BLACK);
    }
}

static void safe_attron(int pair, bool bold, bool blink)
{
    if (!g_has_colors)
        return;
    attron(COLOR_PAIR(pair));
    if (bold)
        attron(A_BOLD);
    if (blink)
        attron(A_BLINK);
}
static void safe_attroff(int pair, bool bold, bool blink)
{
    if (!g_has_colors)
        return;
    attroff(COLOR_PAIR(pair));
    if (bold)
        attroff(A_BOLD);
    if (blink)
        attroff(A_BLINK);
}

// Función auxiliar para verificar si una celda es la posición actual de algún jugador
static bool is_current_player_position(GameState *G, unsigned x, unsigned y)
{
    for (unsigned i = 0; i < G->n_players; i++)
    {
        if (G->P[i].x == x && G->P[i].y == y)
        {
            return true;
        }
    }
    return false;
}

static void draw_board(GameState *G, int start_y, int start_x)
{
    unsigned w = G->w, h = G->h;

    safe_attron(COLOR_UI + 1, true, false);
    mvprintw(start_y - 2, start_x, "Board (%ux%u)", w, h);
    safe_attroff(COLOR_UI + 1, true, false);

    for (unsigned y = 0; y < h; y++)
    {
        for (unsigned x = 0; x < w; x++)
        {
            int cell_start_y = start_y + (int)(y * (CELL_HEIGHT - 1));
            int cell_start_x = start_x + (int)(x * (CELL_WIDTH - 1));

            int v = G->board[idx(G, x, y)];
            int owner = cell_owner(v);

            int color_pair_id = COLOR_REWARD + 0;
            char content_char;
            bool is_owned = false;
            bool is_current_pos = is_current_player_position(G, x, y);

            if (owner >= 0)
            {
                is_owned = true;
                color_pair_id = (is_current_pos ? COLOR_PLAYER_CURRENT : COLOR_PLAYER_BASE) + (owner % 8);
                content_char = (char)('A' + owner);
            }
            else
            {
                int r = cell_reward(v);
                if (r > 9)
                    r = 9; // Limitar a un dígito decimal

                content_char = (char)('0' + r);
            }

            // Relleno interno
            safe_attron(color_pair_id, false, false);
            for (int inner_y = 1; inner_y < CELL_HEIGHT; ++inner_y)
            {
                for (int inner_x = 1; inner_x < CELL_WIDTH; ++inner_x)
                {
                    mvaddch(cell_start_y + inner_y, cell_start_x + inner_x, ' ');
                }
            }
            safe_attroff(color_pair_id, false, false);

            // Contenido centrado
            if (is_owned)
                safe_attron(color_pair_id, true, false);
            else
                safe_attron(color_pair_id, false, false);

            mvaddch(cell_start_y + (CELL_HEIGHT / 2),
                    cell_start_x + (CELL_WIDTH / 2), content_char);
            safe_attroff(color_pair_id, true, false);

            // Bordes
            safe_attron(COLOR_UI + 0, false, false);
            mvaddch(cell_start_y, cell_start_x, ACS_ULCORNER);
            mvaddch(cell_start_y, cell_start_x + CELL_WIDTH - 1, (x == w - 1) ? ACS_URCORNER : ACS_TTEE);
            mvaddch(cell_start_y + CELL_HEIGHT - 1, cell_start_x, (y == h - 1) ? ACS_LLCORNER : ACS_LTEE);
            mvaddch(cell_start_y + CELL_HEIGHT - 1, cell_start_x + CELL_WIDTH - 1,
                    (x == w - 1 && y == h - 1) ? ACS_LRCORNER : ACS_PLUS);

            for (int i = 1; i < CELL_WIDTH - 1; ++i)
            {
                mvaddch(cell_start_y, cell_start_x + i, ACS_HLINE);
                mvaddch(cell_start_y + CELL_HEIGHT - 1, cell_start_x + i, ACS_HLINE);
            }
            for (int i = 1; i < CELL_HEIGHT - 1; ++i)
            {
                mvaddch(cell_start_y + i, cell_start_x, ACS_VLINE);
                mvaddch(cell_start_y + i, cell_start_x + CELL_WIDTH - 1, ACS_VLINE);
            }
            safe_attroff(COLOR_UI + 0, false, false);
        }
    }
}

static void draw_players_info(GameState *G, int start_y, int start_x)
{
    unsigned n = G->n_players;

    safe_attron(COLOR_UI + 1, true, false);
    mvprintw(start_y, start_x, "Players (%u):", n);
    safe_attroff(COLOR_UI + 1, true, false);

    for (unsigned i = 0; i < n; i++)
    {
        int y = start_y + 2 + (int)i;
        int color_pair = COLOR_PLAYER_BASE + (int)(i % 8);

        // Letra del jugador
        safe_attron(color_pair, true, false);
        mvaddch(y, start_x, (char)('A' + i));
        safe_attroff(color_pair, true, false);

        // Info básica
        safe_attron(COLOR_UI + 0, false, false);
        mvprintw(y, start_x + 2, "pos=(%u,%u) score=%u",
                 G->P[i].x, G->P[i].y, G->P[i].score);
        safe_attroff(COLOR_UI + 0, false, false);

        // Stats
        safe_attron(COLOR_UI + 3, false, false);
        mvprintw(y, start_x + 25, "valid=%u", G->P[i].valids);
        safe_attroff(COLOR_UI + 3, false, false);

        safe_attron(COLOR_UI + 2, false, false);
        mvprintw(y, start_x + 35, "invalid=%u", G->P[i].invalids);
        safe_attroff(COLOR_UI + 2, false, false);

        if (G->P[i].blocked)
        {
            safe_attron(COLOR_UI + 2, true, true);
            mvprintw(y, start_x + 48, "[BLOCKED]");
            safe_attroff(COLOR_UI + 2, true, true);
        }
    }
}

static void draw_game_info(GameState *G, int start_y, int start_x)
{
    safe_attron(COLOR_UI + 1, true, false);
    mvprintw(start_y, start_x, "Game Status");
    safe_attroff(COLOR_UI + 1, true, false);

    if (G->game_over)
    {
        safe_attron(COLOR_UI + 2, true, true);
        mvprintw(start_y + 2, start_x, "GAME OVER");
        safe_attroff(COLOR_UI + 2, true, true);
    }
    else
    {
        safe_attron(COLOR_UI + 3, true, false);
        mvprintw(start_y + 2, start_x, "RUNNING");
        safe_attroff(COLOR_UI + 3, true, false);
    }

    safe_attron(COLOR_UI + 0, false, false);
    mvprintw(start_y + 4, start_x, "Board: %ux%u", G->w, G->h);
    mvprintw(start_y + 5, start_x, "Players: %u", G->n_players);
    safe_attroff(COLOR_UI + 0, false, false);
}

static void draw_legend(int start_y, int start_x)
{
    safe_attron(COLOR_UI + 1, true, false);
    mvprintw(start_y, start_x, "Legend:");
    safe_attroff(COLOR_UI + 1, true, false);

    safe_attron(COLOR_UI + 0, false, false);
    mvprintw(start_y + 2, start_x, "A-H : Player territories");
    mvprintw(start_y + 3, start_x, "0-9 : Reward values");
    mvprintw(start_y + 4, start_x, "Press 'q' to quit");
    safe_attroff(COLOR_UI + 0, false, false);
}

int main(int argc, char *argv[])
{
    install_signal_handlers();

    unsigned argW = 0, argH = 0;
    if (argc >= 3)
    {
        argW = (unsigned)atoi(argv[1]);
        argH = (unsigned)atoi(argv[2]);
    }

    // Adjuntar memoria compartida
    GameState *G = state_attach();
    global_state = G;
    if (!G)
    {
        fprintf(stderr, "shm_attach_map view failed\n");
        return 1;
    }

    if (sync_attach() != 0)
    {
        fprintf(stderr, "sync_attach failed\n");
        state_destroy(G);
        return 1;
    }

    // Inicializar ncurses con newterm() para luego poder delscreen()
    global_scr = newterm(NULL, stdout, stdin);
    if (!global_scr)
    {
        fprintf(stderr, "newterm failed\n");
        cleanup_and_exit(1);
    }
    set_term(global_scr);

    ncurses_initialized = 1;
    cbreak();
    noecho();
    nodelay(stdscr, TRUE); // No bloquear en getch()
    keypad(stdscr, TRUE);
    curs_set(0); // Ocultar cursor

    init_colors();

    // Loop principal de renderizado
    int frame = 0;
    int quit_requested = 0;
    while (!g_should_exit && frame < 2000)
    {
    view_wait_update_ready();

        /* Validación opcional de tamaños si vinieron por argv */
        if (argW && argH && (G->w != argW || G->h != argH))
        {
            /* Solo advertir una vez al primer frame para evitar spam */
            static int warned = 0;
            if (!warned)
            {
                fprintf(stderr, "view_ncurses: aviso: tamaño SHM=%ux%u difiere de argv=%ux%u\n",
                        G->w, G->h, argW, argH);
                warned = 1;
            }
        }

        // Input no bloqueante
        int ch = getch();
        if (ch == 'q' || ch == 'Q')
        {
            quit_requested = 1;
        }

        // Limpiar pantalla
        clear();

        // Dimensiones terminal
        int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);

        // Layout: texto arriba, tablero abajo, panel a la derecha
        int top_text_height = 10; // altura reservada para texto (arriba)
        if (top_text_height > max_y - 5)
            top_text_height = max_y - 5; // evitar superposición

        int board_start_y = top_text_height; // arranca debajo del bloque de texto
        int board_start_x = 2;
        int board_height = max_y - top_text_height - 2;
        int board_width = max_x - 35;
        if (board_height < 5)
            board_height = 5;
        if (board_width < 20)
            board_width = 20;

        // Dibujos
        draw_board(G, board_start_y, board_start_x);

        int panel_x = board_start_x + board_width + 2;
        // Texto arriba (status y leyenda)
        draw_game_info(G, 1, panel_x);
        draw_legend(6, panel_x);

        // Info de jugadores también en el bloque superior
        int players_y = 1;
        draw_players_info(G, players_y, board_start_x);

        safe_attron(COLOR_UI + 0, false, false);
        mvprintw(max_y - 1, 0, "Frame: %d | Press 'q' to quit", frame);
        safe_attroff(COLOR_UI + 0, false, false);

    int game_over_now = G->game_over ? 1 : 0;

        // Actualizar pantalla
        refresh();
        view_signal_render_complete();

        if (game_over_now || quit_requested)
            break;

        /* Sin sleep aquí: el master controla el pacing con -d. */
        frame++;
    }

    cleanup_and_exit(0);
    return 0; // no se alcanza
}
