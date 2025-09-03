#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <string.h>
#include "master_logic.h"

static void print_usage(const char *prog) {
    fprintf(stderr,
        "Uso: %s [-w width] [-h height] [-d delay_ms] [-t timeout] [-s seed] "
        "[-v ./view] -p player1 [player2 ...]\n", prog);
}

int parse_args(int argc, char *argv[], MasterConfig *config)
{
    /* Defaults razonables */
    config->width  = 10;
    config->height = 10;
    config->delay  = 300;               /* 300 ms de poll por defecto */
    config->timeout = 10;               /* reservado / no usado aún */
    config->seed   = (unsigned int)time(NULL);
    config->view_path = NULL;
    config->player_count = 0;

    for (int i = 0; i < 9; ++i) config->player_paths[i] = NULL;

    opterr = 0;  /* silenciar mensajes automáticos de getopt */
    optind = 1;

    int opt;
    /* Opciones cortas: las que llevan ':' requieren argumento */
    while ((opt = getopt(argc, argv, "w:h:d:t:s:v:p:")) != -1) {
        switch (opt) {
        case 'w': config->width  = atoi(optarg); break;
        case 'h': config->height = atoi(optarg); break;
        case 'd': config->delay  = atoi(optarg); break;
        case 't': config->timeout= atoi(optarg); break;
        case 's': config->seed   = (unsigned int)atoi(optarg); break;
        case 'v': config->view_path = optarg; break;

        case 'p':
            /* Permitir varios players después de -p, hasta otro flag o fin */
            optind--; /* incluir el primero (optarg) en el loop manual */
            while (optind < argc &&
                   argv[optind][0] != '-' &&
                   config->player_count < 9)
            {
                config->player_paths[config->player_count++] = argv[optind];
                optind++;
            }
            break;

        default:
            print_usage(argv[0]);
            return -1;
        }
    }

    /* Validaciones mínimas */
    if (config->player_count == 0) {
        fprintf(stderr, "Error: se requiere al menos un jugador con -p.\n");
        print_usage(argv[0]);
        return -1;
    }
    if (config->width <= 0 || config->height <= 0) {
        fprintf(stderr, "Error: width/height deben ser > 0.\n");
        return -1;
    }
    if (config->delay < 0) config->delay = 0;

    return 0;
}
