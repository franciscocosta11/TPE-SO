.RECIPEPREFIX := >
CC=gcc
CFLAGS=-std=c11 -O2 -Wall -Wextra -Werror -pedantic -Iinclude
LDFLAGS=-pthread 
ifeq ($(UNAME_S),Linux)
  LDFLAGS += -lrt
endif

SRC_COMMON=src/common/state.c src/common/rules.c src/common/sync.c src/common/shm.c src/common/state_access.c
OBJ_COMMON=$(SRC_COMMON:.c=.o)

# master-specific sources
SRC_MASTER=src/master/master_logic.c
OBJ_MASTER=$(SRC_MASTER:.c=.o)

all: master player view_ncurses
# (no agrego los test a 'all' para no tocar tu flujo normal)

master: src/master/main.c $(OBJ_COMMON) $(OBJ_MASTER)
> $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

view_ncurses: src/view/view_ncurses.c $(OBJ_COMMON)
> $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lncurses

player: src/player/main.c $(OBJ_COMMON)
> $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

init_state: tests/init_state.c $(OBJ_COMMON)
> $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

writer_tick: tests/writer_tick.c $(OBJ_COMMON)
> $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

src/common/%.o: src/common/%.c
> $(CC) $(CFLAGS) -c -o $@ $<

src/master/%.o: src/master/%.c
> $(CC) $(CFLAGS) -c -o $@ $<

clean:
> rm -f master player view_ncurses $(OBJ_COMMON) src/master/*.o

.PHONY: all clean

# Valgrind targets
VALGRIND ?= valgrind
VGFLAGS  := --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes --errors-for-leak-kinds=all
VG_TIMEOUT ?= 60s

.PHONY: valgrind_master

valgrind_master: master player view_ncurses
> @echo "Preparing ./logs directory and running master under valgrind (per-process logs: ./logs/valgrind.<pid>.log) ..."
> mkdir -p ./logs
> timeout $(VG_TIMEOUT) $(VALGRIND) $(VGFLAGS) --trace-children=yes -s --log-file=./logs/valgrind.%p.log -- ./master -v ./view_ncurses -p ./player ./player || true
> @echo "Done. Check ./logs/ for valgrind.*.log"