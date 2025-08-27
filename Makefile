.RECIPEPREFIX := >
CC=gcc
CFLAGS=-std=c11 -O2 -Wall -Wextra -Werror -pedantic -Iinclude
LDFLAGS=-pthread -lrt

# CuTest
CUTEST_DIR=CuTest
CUTEST_REQ=$(CUTEST_DIR)/AllTests.c $(CUTEST_DIR)/CuTestTest.c

# 👇 Agrego sync.c acá
SRC_COMMON=src/common/util.c src/common/state.c src/common/rules.c src/common/sync.c
OBJ_COMMON=$(SRC_COMMON:.c=.o)

# Archivos de lógica del master (el código compartido)
SRC_MASTER_LOGIC=src/master/master_logic.c
OBJ_MASTER_LOGIC=$(SRC_MASTER_LOGIC:.c=.o)

# --- CONFIGURACIÓN DE CUTEST ---
CUTEST_DIR=CuTest
SRC_CUTEST=$(CUTEST_DIR)/CuTest.c
OBJ_CUTEST=$(SRC_CUTEST:.c=.o)

# --- EJECUTABLES ---
TEST_BINS=test_master

# ==============================================================================
# --- TARGETS PRINCIPALES ---
# ==============================================================================
all: master view player

# El programa 'master' depende de su 'main', la lógica compartida y lo común
master: src/master/main.c $(OBJ_MASTER_LOGIC) $(OBJ_COMMON)
> $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

view: src/view/main.c $(OBJ_COMMON)
> $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

player: src/player/main.c $(filter-out src/common/sync.o,$(OBJ_COMMON))
> $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

src/common/%.o: src/common/%.c
> $(CC) $(CFLAGS) -c -o $@ $<

test:
> $(MAKE) -C $(CUTEST_DIR) test

clean:
> rm -f master view player $(OBJ_COMMON) $(OBJ_MASTER_LOGIC) $(OBJ_CUTEST) $(TEST_BINS)

.PHONY: all clean test

.PHONY: all clean cutest test
