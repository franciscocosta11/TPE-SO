.RECIPEPREFIX := >
CC=gcc
CFLAGS=-std=c11 -O2 -Wall -Wextra -Werror -pedantic -Iinclude
LDFLAGS=-pthread -lrt

# CuTest
CUTEST_DIR=CuTest
CUTEST_REQ=$(CUTEST_DIR)/AllTests.c $(CUTEST_DIR)/CuTestTest.c

SRC_COMMON=src/common/util.c src/common/state.c src/common/rules.c src/common/sync.c src/common/shm.c src/common/state_access.c
OBJ_COMMON=$(SRC_COMMON:.c=.o)

# master-specific sources
SRC_MASTER=src/master/master_logic.c
OBJ_MASTER=$(SRC_MASTER:.c=.o)

all: master view player
# (no agrego los test a 'all' para no tocar tu flujo normal)

master: src/master/main.c $(OBJ_COMMON) $(OBJ_MASTER)
> $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

view: src/view/main.c $(OBJ_COMMON)
> $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

player: src/player/main.c $(OBJ_COMMON)
> $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

init_state: tests/init_state.c $(OBJ_COMMON)
> $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# ---- NUEVO: builds de stress (Día 3 - C) ----
reader_stress: tests/reader_stress.c $(OBJ_COMMON)
> $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

writer_tick: tests/writer_tick.c $(OBJ_COMMON)
> $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Opcional: target para lanzar el script de stress si existe
stress: reader_stress writer_tick
> if [ -x scripts/stress.sh ]; then scripts/stress.sh; else echo "Tip: creá scripts/stress.sh y hacelo ejecutable"; fi

src/common/%.o: src/common/%.c
> $(CC) $(CFLAGS) -c -o $@ $<

src/master/%.o: src/master/%.c
> $(CC) $(CFLAGS) -c -o $@ $<

test:
> $(MAKE) -C $(CUTEST_DIR) test

clean:
> rm -f master view player reader_stress writer_tick $(OBJ_COMMON)
> $(MAKE) -C $(CUTEST_DIR) clean

.PHONY: all clean cutest test stress
