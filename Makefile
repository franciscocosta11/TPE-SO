.RECIPEPREFIX := >
CC=gcc
CFLAGS=-std=c11 -O2 -Wall -Wextra -Werror -pedantic -Iinclude
LDFLAGS=-pthread 
ifeq ($(UNAME_S),Linux)
  LDFLAGS += -lrt
endif
# CuTest
CUTEST_DIR=CuTest
CUTEST_REQ=$(CUTEST_DIR)/AllTests.c $(CUTEST_DIR)/CuTestTest.c

SRC_COMMON=src/common/log.c src/common/state.c src/common/rules.c src/common/sync.c src/common/shm.c src/common/state_access.c
OBJ_COMMON=$(SRC_COMMON:.c=.o)

# master-specific sources
SRC_MASTER=src/master/master_logic.c
OBJ_MASTER=$(SRC_MASTER:.c=.o)

all: master player view_ncurses
# (no agrego los test a 'all' para no tocar tu flujo normal)

master: src/master/main.c $(OBJ_COMMON) $(OBJ_MASTER)
> $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

view: src/view/main.c $(OBJ_COMMON)
> $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

view_ncurses: src/view/view_ncurses.c $(OBJ_COMMON)
> $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lncurses

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

# Valgrind targets
VALGRIND ?= valgrind
VGFLAGS  := --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes --errors-for-leak-kinds=all
VG_TIMEOUT ?= 60s

.PHONY: valgrind_master valgrind_tests

valgrind_master: master player view_ncurses
> @echo "Preparing ./logs directory and running master under valgrind (per-process logs: ./logs/valgrind.<pid>.log) ..."
> mkdir -p ./logs
> timeout $(VG_TIMEOUT) $(VALGRIND) $(VGFLAGS) --trace-children=yes --log-file=./logs/valgrind.%p.log -- ./master -v ./view_ncurses -p ./player ./player || true
> @echo "Done. Check ./logs/ for valgrind.*.log"

valgrind_tests: reader_stress writer_tick
> @echo "Running writer_tick under valgrind (log: /tmp/valgrind.writer_tick.log) ..."
> timeout 20s $(VALGRIND) $(VGFLAGS) --log-file=/tmp/valgrind.writer_tick.log -- ./writer_tick || true
> @echo "Running reader_stress under valgrind (log: /tmp/valgrind.reader_stress.log) ..."
> timeout 20s $(VALGRIND) $(VGFLAGS) --log-file=/tmp/valgrind.reader_stress.log -- ./reader_stress || true
> @echo "Done. Check /tmp/valgrind.*.log"

.PHONY: valgrind_view

valgrind_view: view_ncurses init_state
> @echo "Running init_state to create shared memory and semaphores"
> ./init_state
> @echo "Running view_ncurses under valgrind with ncurses suppressions -> ./logs/valgrind_view.log"
> mkdir -p ./logs
> $(VALGRIND) $(VGFLAGS) --suppressions=./logs/valgrind_ncurses.supp --log-file=./logs/valgrind_view.log ./view_ncurses || true
> @echo "Done. See ./logs/valgrind_view.log"
