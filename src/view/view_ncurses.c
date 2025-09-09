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
#include "shm.h"
#include "state.h"
#include "state_access.h"
#include "sync.h"

// Colores para los jugadores
#define COLOR_PLAYER_BASE 10
#define COLOR_REWARD 20
#define COLOR_UI 30
#define CELL_HEIGHT 3
#define CELL_WIDTH 5

// Variables globales para cleanup
static GameState *global_state = NULL;
static int ncurses_initialized = 0;

static void cleanup_and_exit()
{
    if (ncurses_initialized)
    {
        endwin();
    }
    if (global_state)
    {
        state_destroy(global_state);
    }
    exit(0);
}

static void msleep(int ms)
{
    struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}

static void init_colors(void)
{
    if (has_colors())
    {
        start_color();

        // Colores para jugadores (fondo coloreado, texto negro)
        init_pair(COLOR_PLAYER_BASE + 0, COLOR_BLACK, COLOR_RED);     // Jugador 0
        init_pair(COLOR_PLAYER_BASE + 1, COLOR_BLACK, COLOR_GREEN);   // Jugador 1
        init_pair(COLOR_PLAYER_BASE + 2, COLOR_BLACK, COLOR_YELLOW);  // Jugador 2
        init_pair(COLOR_PLAYER_BASE + 3, COLOR_BLACK, COLOR_BLUE);    // Jugador 3
        init_pair(COLOR_PLAYER_BASE + 4, COLOR_BLACK, COLOR_MAGENTA); // Jugador 4
        init_pair(COLOR_PLAYER_BASE + 5, COLOR_BLACK, COLOR_CYAN);    // Jugador 5
        init_pair(COLOR_PLAYER_BASE + 6, COLOR_WHITE, COLOR_RED);     // Jugador 6
        init_pair(COLOR_PLAYER_BASE + 7, COLOR_WHITE, COLOR_GREEN);   // Jugador 7

        // Colores para UI
        init_pair(COLOR_UI + 0, COLOR_WHITE, COLOR_BLACK);  // Normal
        init_pair(COLOR_UI + 1, COLOR_YELLOW, COLOR_BLACK); // Destacado
        init_pair(COLOR_UI + 2, COLOR_RED, COLOR_BLACK);    // Error/Blocked
        init_pair(COLOR_UI + 3, COLOR_GREEN, COLOR_BLACK);  // Éxito
    }
}

static void draw_board(GameState *G, int start_y, int start_x)
{
    unsigned w = G->w, h = G->h;

    attron(COLOR_PAIR(COLOR_UI + 1) | A_BOLD);
    mvprintw(start_y - 2, start_x, "Board (%ux%u)", w, h);
    attroff(COLOR_PAIR(COLOR_UI + 1) | A_BOLD);

    for (unsigned y = 0; y < h; y++)
    {
        for (unsigned x = 0; x < w; x++)
        {
            int cell_start_y = start_y + (y * (CELL_HEIGHT - 1));
            int cell_start_x = start_x + (x * (CELL_WIDTH - 1));

            int v = G->board[idx(G, x, y)];
            int owner = cell_owner(v);

            int color_pair_id;
            char content_char;
            bool is_owned = false; // Para saber si pintar el fondo

            if (owner >= 0)
            {
                is_owned = true;
                color_pair_id = COLOR_PLAYER_BASE + (owner % 8);
                content_char = 'A' + owner;
            }
            else
            {
                int r = cell_reward(v);
                if (r < 0)
                    r = 0;
                if (r > 9)
                    r = 9;

                content_char = '0' + r;
                color_pair_id = COLOR_REWARD + 0; // Color por defecto (blanco sobre negro)
            }

            attron(COLOR_PAIR(color_pair_id));

            // --- CAMBIO 3: Pintar fondo solo si la celda está ocupada ---
            if (is_owned)
            {
                for (int inner_y = 1; inner_y < CELL_HEIGHT - 1; ++inner_y)
                {
                    for (int inner_x = 1; inner_x < CELL_WIDTH - 1; ++inner_x)
                    {
                        mvaddch(cell_start_y + inner_y, cell_start_x + inner_x, ' ');
                    }
                }
            }

            // Centrar el contenido
            mvaddch(cell_start_y + (CELL_HEIGHT / 2), cell_start_x + (CELL_WIDTH / 2), content_char);

            // Dibujar bordes (con caracteres especiales para uniones)
            // Esquinas
            mvaddch(cell_start_y, cell_start_x, ACS_ULCORNER);
            mvaddch(cell_start_y, cell_start_x + CELL_WIDTH - 1, x == w - 1 ? ACS_URCORNER : ACS_TTEE);
            mvaddch(cell_start_y + CELL_HEIGHT - 1, cell_start_x, y == h - 1 ? ACS_LLCORNER : ACS_LTEE);
            mvaddch(cell_start_y + CELL_HEIGHT - 1, cell_start_x + CELL_WIDTH - 1, (x == w - 1 && y == h - 1) ? ACS_LRCORNER : ACS_PLUS);

            // Líneas horizontales
            for (int i = 1; i < CELL_WIDTH - 1; ++i)
            {
                mvaddch(cell_start_y, cell_start_x + i, ACS_HLINE);
                if (y < h - 1)
                { // Solo dibuja líneas inferiores si no es la última fila
                    mvaddch(cell_start_y + CELL_HEIGHT - 1, cell_start_x + i, ACS_HLINE);
                }
            }

            // Líneas verticales
            for (int i = 1; i < CELL_HEIGHT - 1; ++i)
            {
                mvaddch(cell_start_y + i, cell_start_x, ACS_VLINE);
                if (x < w - 1)
                { // Solo dibuja líneas derechas si no es la última columna
                    mvaddch(cell_start_y + i, cell_start_x + CELL_WIDTH - 1, ACS_VLINE);
                }
            }

            attroff(COLOR_PAIR(color_pair_id));
        }
    }
}

static void draw_players_info(GameState *G, int start_y, int start_x)
{
    unsigned n = G->n_players;

    attron(COLOR_PAIR(COLOR_UI + 1) | A_BOLD);
    mvprintw(start_y, start_x, "Players (%u):", n);
    attroff(COLOR_PAIR(COLOR_UI + 1) | A_BOLD);

    for (unsigned i = 0; i < n; i++)
    {
        int y = start_y + 2 + i;
        int color_pair = COLOR_PLAYER_BASE + (i % 8);

        // Mostrar letra del jugador con su color
        attron(COLOR_PAIR(color_pair) | A_BOLD);
        mvaddch(y, start_x, 'A' + i);
        attroff(COLOR_PAIR(color_pair) | A_BOLD);

        // Información del jugador
        attron(COLOR_PAIR(COLOR_UI + 0));
        mvprintw(y, start_x + 2, "pos=(%u,%u) score=%u",
                 G->P[i].x, G->P[i].y, G->P[i].score);
        attroff(COLOR_PAIR(COLOR_UI + 0));

        // Estadísticas de movimientos
        attron(COLOR_PAIR(COLOR_UI + 3));
        mvprintw(y, start_x + 25, "valid=%u", G->P[i].valids);
        attroff(COLOR_PAIR(COLOR_UI + 3));

        attron(COLOR_PAIR(COLOR_UI + 2));
        mvprintw(y, start_x + 35, "invalid=%u", G->P[i].invalids);
        attroff(COLOR_PAIR(COLOR_UI + 2));

        // Estado bloqueado
        if (G->P[i].blocked)
        {
            attron(COLOR_PAIR(COLOR_UI + 2) | A_BOLD | A_BLINK);
            mvprintw(y, start_x + 48, "[BLOCKED]");
            attroff(COLOR_PAIR(COLOR_UI + 2) | A_BOLD | A_BLINK);
        }
    }
}

static void draw_game_info(GameState *G, int start_y, int start_x)
{
    // Información general del juego
    attron(COLOR_PAIR(COLOR_UI + 1) | A_BOLD);
    mvprintw(start_y, start_x, "Game Status");
    attroff(COLOR_PAIR(COLOR_UI + 1) | A_BOLD);

    // Estado del juego
    if (G->game_over)
    {
        attron(COLOR_PAIR(COLOR_UI + 2) | A_BOLD | A_BLINK);
        mvprintw(start_y + 2, start_x, "GAME OVER");
        attroff(COLOR_PAIR(COLOR_UI + 2) | A_BOLD | A_BLINK);
    }
    else
    {
        attron(COLOR_PAIR(COLOR_UI + 3) | A_BOLD);
        mvprintw(start_y + 2, start_x, "RUNNING");
        attroff(COLOR_PAIR(COLOR_UI + 3) | A_BOLD);
    }

    // Dimensiones del tablero
    attron(COLOR_PAIR(COLOR_UI + 0));
    mvprintw(start_y + 4, start_x, "Board: %ux%u", G->w, G->h);
    mvprintw(start_y + 5, start_x, "Players: %u", G->n_players);
    attroff(COLOR_PAIR(COLOR_UI + 0));
}

static void draw_legend(int start_y, int start_x)
{
    attron(COLOR_PAIR(COLOR_UI + 1) | A_BOLD);
    mvprintw(start_y, start_x, "Legend:");
    attroff(COLOR_PAIR(COLOR_UI + 1) | A_BOLD);

    attron(COLOR_PAIR(COLOR_UI + 0));
    mvprintw(start_y + 2, start_x, "A-H : Player territories");
    mvprintw(start_y + 3, start_x, "0-9 : Reward values");
    mvprintw(start_y + 4, start_x, "Press 'q' to quit");
    attroff(COLOR_PAIR(COLOR_UI + 0));
}

int main(void)
{
    // Configurar manejo de señales
    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);

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

    // Inicializar ncurses
    initscr();
    ncurses_initialized = 1;
    cbreak();
    noecho();
    nodelay(stdscr, TRUE); // No bloquear en getch()
    keypad(stdscr, TRUE);
    curs_set(0); // Ocultar cursor

    init_colors();

    // Loop principal de renderizado
    int frame = 0;
    while (frame < 2000)
    { // Más frames para una sesión más larga
        view_wait_update_ready();
        rdlock();

        // Verificar si el usuario presionó 'q' para salir
        int ch = getch();
        if (ch == 'q' || ch == 'Q')
        {
            rdunlock();
            break;
        }

        // Limpiar pantalla
        clear();

        // Obtener dimensiones de la terminal
        int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);

        // Calcular layout
        int board_start_y = 1;
        int board_start_x = 2;
        int board_height = max_y - 15; // Dejar espacio para info de jugadores
        int board_width = max_x - 35;  // Dejar espacio para panel lateral

        if (board_height < 5)
            board_height = 5;
        if (board_width < 20)
            board_width = 20;

        // Dibujar tablero
        draw_board(G, board_start_y, board_start_x);

        // Panel lateral derecho
        int panel_x = board_start_x + board_width + 2;

        // Información del juego
        draw_game_info(G, 1, panel_x);

        // Leyenda
        draw_legend(8, panel_x);

        // Información de jugadores (parte inferior)
        int players_y = max_y - 8; // Posición fija cerca del fondo
        draw_players_info(G, players_y, board_start_x);

        // Información de frame en la esquina
        attron(COLOR_PAIR(COLOR_UI + 0));
        mvprintw(max_y - 1, 0, "Frame: %d | Press 'q' to quit", frame);
        attroff(COLOR_PAIR(COLOR_UI + 0));

        rdunlock();
        
        // Actualizar pantalla
        refresh();
        view_signal_render_complete();

        // Control de FPS (~10 fps)
        msleep(100);
        frame++;

        // Salir si el juego terminó
        if (G->game_over)
        {
            // Mostrar mensaje final por unos segundos más
            for (int i = 0; i < 30; i++)
            {
                int ch = getch();
                if (ch == 'q' || ch == 'Q')
                    break;
                msleep(100);
            }
            break;
        }
    }

    cleanup_and_exit(0);
    return 0;
}