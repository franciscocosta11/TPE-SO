.RECIPEPREFIX := >
CC=gcc
# ---> [IMPORTANTE] Agregamos 'src/master' a las rutas de inclusión
CFLAGS=-std=c11 -O2 -Wall -Wextra -Werror -pedantic -Iinclude -ICuTest -Isrc/master
LDFLAGS=-pthread

# --- ARCHIVOS FUENTE ---
SRC_COMMON=src/common/util.c
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

player: src/player/main.c $(OBJ_COMMON)
> $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# ==============================================================================
# --- TARGETS DE TEST ---
# ==============================================================================
test: $(TEST_BINS)
> ./test_master

# El test del master depende de su código de test, la lógica compartida y lo común
test_master: src/master/test_master.c $(OBJ_MASTER_LOGIC) $(OBJ_COMMON) $(OBJ_CUTEST)
> $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# ==============================================================================
# --- REGLAS DE COMPILACIÓN GENÉRICAS ---
# ==============================================================================
src/master/%.o: src/master/%.c
> $(CC) $(CFLAGS) -c -o $@ $<

src/common/%.o: src/common/%.c
> $(CC) $(CFLAGS) -c -o $@ $<

$(CUTEST_DIR)/%.o: $(CUTEST_DIR)/%.c
> $(CC) $(CFLAGS) -c -o $@ $<

# ==============================================================================
# --- LIMPIEZA ---
# ==============================================================================
clean:
> rm -f master view player $(OBJ_COMMON) $(OBJ_MASTER_LOGIC) $(OBJ_CUTEST) $(TEST_BINS)

.PHONY: all clean test
