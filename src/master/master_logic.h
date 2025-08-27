// en src/master/master_logic.h

#ifndef MASTER_LOGIC_H
#define MASTER_LOGIC_H

#include "shared_data.h" // Necesitamos la definición de MasterConfig

// Esta es la única línea que importa: la declaración de la función.
int parse_args(int argc, char *argv[], MasterConfig *config);

#endif // MASTER_LOGIC_H