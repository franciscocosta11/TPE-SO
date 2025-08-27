#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include "unistd.h"
#include "shared_data.h" // Asegúrate de que este header define MasterConfig
#include <time.h>
#include "master_logic.h"

int parse_args(int argc, char *argv[], MasterConfig *config)
{
    config->width = 10;
    config->height = 10;
    config->delay = 200;
    config->timeout = 10;
    config->seed = time(NULL); // O un valor fijo para tests
    config->view_path = NULL;
    config->player_count = 0;

    optind = 1;
    opterr = 0;
    int opt;
    // El string "w:h:d:t:s:v:p:" le dice a getopt qué opciones esperas
    // y cuáles requieren un argumento (las que tienen ':')
    while ((opt = getopt(argc, argv, "w:h:d:t:s:v:p:")) != -1)
    {
        switch (opt)
        {
        case 'w':
            config->width = atoi(optarg);
            break;
        case 'h':
            config->height = atoi(optarg);
            break;
        case 'd':
            config->delay = atoi(optarg);
            break;
        case 't':
            config->timeout = atoi(optarg);
            break;
        case 's':
            config->seed = (unsigned int)atoi(optarg);
            break;
        case 'v':
            config->view_path = optarg;
            break;
        case 'p':
            // 'p' es especial porque puede haber varios jugadores.
            // Reiniciamos el índice de getopt para leer todos los jugadores.
            optind--; // Retrocedemos el índice para incluir el primer jugador
            while (optind < argc && argv[optind][0] != '-' && config->player_count < 9)
            {
                config->player_paths[config->player_count] = argv[optind];
                config->player_count++;
                optind++;
            }
            break;
        default: /* '?' */
            fprintf(stderr, "Uso: %s [-w width] [-h height] -p player1 [player2...]\n", argv[0]);
            return -1; // Error
        }
    }

    // Validar que haya al menos un jugador
    if (config->player_count == 0)
    {
        fprintf(stderr, "Error: Se requiere al menos un jugador con -p.\n");
        return -1;
    }

    return 0; // Éxito
}