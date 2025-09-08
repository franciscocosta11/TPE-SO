#ifndef MASTER_LOGIC_H
#define MASTER_LOGIC_H

typedef struct {
    int width;                  /* ancho del tablero */
    int height;                 /* alto del tablero */
    int delay;                  /* delay de poll en ms (select) */
    int timeout;                /* timeout de ronda en ms (0 = deshabilitado) */
    int player_timeout_ms;      /* NUEVO: timeout individual por jugador en ms (0 = deshabilitado) */
    unsigned int seed;          /* semilla de RNG */
    char *view_path;            /* path al ejecutable view (opcional) */
    char *player_paths[9];      /* paths a ejecutables player */
    int player_count;           /* cantidad de players */
} MasterConfig;

/* Parsea argv y completa config con defaults + overrides. Devuelve 0 si ok. */
int parse_args(int argc, char *argv[], MasterConfig *config);

#endif /* MASTER_LOGIC_H */
