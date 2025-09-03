#ifndef STATE_ACCESS_H
#define STATE_ACCESS_H

#include <stdbool.h>
#include "state.h"
#include "sync.h"

// Lectura
static inline void state_read_begin(void)  { rdlock(); }
static inline void state_read_end(void)    { rdunlock(); }

// Escritura
static inline void state_write_begin(void) { wrlock(); }
static inline void state_write_end(void)   { wrunlock(); }

// Helpers de lectura comunes (opcional)
unsigned state_get_w(const GameState *G);
unsigned state_get_h(const GameState *G);
unsigned state_get_n(const GameState *G);
bool     state_is_over(const GameState *G);

// Ejemplo: contar recompensas remanentes (opcional)
int state_remaining_rewards(const GameState *G);

#endif
