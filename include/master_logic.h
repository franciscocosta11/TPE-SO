#ifndef MASTER_LOGIC_H
#define MASTER_LOGIC_H


/**
 * @brief Configuración del master (parámetros de ejecución).
 */
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

/**
 * @brief Parsea argv y completa config con defaults + overrides.
 * @param argc Count de argumentos.
 * @param argv Vector de argumentos.
 * @param config Puntero a MasterConfig que será rellenado.
 * @return 0 si OK, -1 en caso de error (mensaje en stderr).
 */
int parse_args(int argc, char *argv[], MasterConfig *config);

#endif // MASTER_LOGIC_H 
