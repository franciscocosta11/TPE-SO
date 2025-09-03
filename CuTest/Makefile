# I am not very good at Makefiles.

CFLAGS += -Wall -g
INCLUDES = -I. -I../include

TEST_SRCS=AllTests.c CuTest.c ../tests/test_state.c ../src/common/state.c ../src/common/shm.c

all: bin/tests

bin:
	mkdir -p $@

test: bin/tests
	@bin/tests

bin/tests: $(TEST_SRCS) | bin
	$(CC) $(CFLAGS) $(INCLUDES) -Wno-address -lm -o $@ $(TEST_SRCS)

clean:
	@rm -rf *~ bin