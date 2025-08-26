.RECIPEPREFIX := >
CC=gcc
CFLAGS=-std=c11 -O2 -Wall -Wextra -Werror -pedantic
CPPFLAGS=-Iinclude
LDFLAGS=-pthread

# CuTest (lo dejo como lo tenías)
CUTEST_DIR=CuTest
CUTEST_REQ=$(CUTEST_DIR)/AllTests.c $(CUTEST_DIR)/CuTestTest.c

# Comunes (agrego log.c a lo que ya tenías)
SRC_COMMON=src/common/util.c src/common/log.c
OBJ_COMMON=$(SRC_COMMON:.c=.o)

# ======================================
# Targets principales
# ======================================
all: master view player

master: src/master/main.c $(OBJ_COMMON)
> $(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

view: src/view/main.c $(OBJ_COMMON)
> $(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

player: src/player/main.c $(OBJ_COMMON)
> $(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

# ======================================
# Objetos comunes
# ======================================
src/common/%.o: src/common/%.c
> $(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

# ======================================
# Atajos de desarrollo (día 2)
# ======================================
# Compila y corre la vista (usa estado dummy si no existe /game_state)
dev: view
> ./view

run_view: view
> ./view

# ======================================
# Limpieza
# ======================================
clean:
> rm -f master view player $(OBJ_COMMON)
> $(MAKE) -C $(CUTEST_DIR) clean

CPPFLAGS=-Iinclude -D_POSIX_C_SOURCE=200809L

.PHONY: all clean dev run_view
