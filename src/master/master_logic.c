// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
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
        "Uso: %s "
        "[-w width] [-h height] "
        "[-d delay_ms] [-t timeout_s] "
        "[-s seed] [-v ./view] "
        "-p player1 [player2 ...]\n\n"
        "Notas:\n"
        "- width/height: mínimo 10 (default 10).\n"
        "- d: delay entre impresiones en ms (default 200).\n"
        "- t: timeout para movimientos válidos en segundos (default 10s).\n"
        "- v: ruta de la vista (por ejemplo ./view_ncurses).\n"
        "- p: entre 1 y 9 jugadores.\n",
        prog);
}

int parse_args(int argc, char *argv[], MasterConfig *config)
{
    config->width  = 10;
    config->height = 10;
    config->delay  = 200;               /* default 200ms */
    config->timeout = 10 * 1000;        /* default 10s en ms */
    config->player_timeout_ms = 0;      /* 0 = sin timeout individual */
    config->seed   = (unsigned int)time(NULL);
    config->view_path = NULL;
    config->player_count = 0;
    for (int i = 0; i < 9; ++i) config->player_paths[i] = NULL;

    opterr = 0;
    optind = 1;

    int opt;
    while ((opt = getopt(argc, argv, "w:h:d:t:T:s:v:p:")) != -1) {
        switch (opt) {
        case 'w': config->width  = atoi(optarg); break;
        case 'h': config->height = atoi(optarg); break;
        case 'd': config->delay  = atoi(optarg); break;
        case 't': {
            int tsec = atoi(optarg);
            if (tsec < 0) tsec = 0;
            config->timeout = tsec * 1000; /* guardar en ms */
            break;
        }
        case 'T': config->player_timeout_ms = atoi(optarg); break;  /* jugador */
        case 's': config->seed   = (unsigned int)atoi(optarg); break;
        case 'v': config->view_path = optarg; break;
        case 'p':
            /* Consumir una lista de rutas hasta el próximo flag o fin. */
            optind--;
            while (optind < argc && argv[optind][0] != '-')
            {
                if (config->player_count >= 9)
                {
                    fprintf(stderr, "Error: máximo 9 jugadores. Argumento extra: '%s'\n", argv[optind]);
                    return -1;
                }
                config->player_paths[config->player_count++] = argv[optind];
                optind++;
            }
            break;
        default:
            print_usage(argv[0]);
            return -1;
        }
    }

    if (config->player_count == 0) {
        fprintf(stderr, "Error: se requiere al menos un jugador con -p.\n");
        print_usage(argv[0]);
        return -1;
    }
    if (config->player_count >= 10) {
        fprintf(stderr, "Error: máximo 9 jugadores. Se pasaron %d.\n", config->player_count);
        return -1;
    }
    if (config->width < 10 || config->height < 10) {
        fprintf(stderr, "Error: width/height deben ser >= 10.\n");
        return -1;
    }
    if (config->delay < 0) config->delay = 0;
    if (config->timeout < 0) config->timeout = 0;
    if (config->player_timeout_ms < 0) config->player_timeout_ms = 0;
    return 0;
}
