.RECIPEPREFIX := >
CC=gcc
CFLAGS=-std=c11 -O2 -Wall -Wextra -Werror -pedantic -Iinclude
LDFLAGS=-pthread 
ifeq ($(UNAME_S),Linux)
  LDFLAGS += -lrt
endif

SRC_COMMON=src/common/state.c src/common/rules.c src/common/sync.c src/common/shm.c src/common/state_access.c
OBJ_COMMON=$(SRC_COMMON:.c=.o)

SRC_MASTER=src/master/master_logic.c
OBJ_MASTER=$(SRC_MASTER:.c=.o)

all: master player player2 view_ncurses

master: src/master/main.c $(OBJ_COMMON) $(OBJ_MASTER)
> $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

view_ncurses: src/view/view_ncurses.c $(OBJ_COMMON)
> $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lncurses

player: src/player/main.c $(OBJ_COMMON)
> $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

player2: src/player/main2.c $(OBJ_COMMON)
> $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

src/common/%.o: src/common/%.c
> $(CC) $(CFLAGS) -c -o $@ $<

src/master/%.o: src/master/%.c
> $(CC) $(CFLAGS) -c -o $@ $<

clean:
> rm -f master player player2 view_ncurses $(OBJ_COMMON) src/master/*.o

.PHONY: all clean
