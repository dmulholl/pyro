# --------- #
# Variables #
# --------- #

CFLAGS = -Wall -Wextra --std=c11 --pedantic -Wno-unused-parameter -Wno-unused-function

SGC = -D PYRO_DEBUG_STRESS_GC

LGC = -D PYRO_DEBUG_LOG_GC

TRACE = -D PYRO_DEBUG_TRACE_EXECUTION -D PYRO_DEBUG_DUMP_BYTECODE

FILES = src/vm/*.c src/std/*.c src/cli/*.c src/lib/mt64/*.c src/lib/args/*.c

# ------------- #
# Phony Targets #
# ------------- #

# Standard release build.
release::
	@mkdir -p out
	$(CC) $(CFLAGS) -D NDEBUG -o out/pyro $(FILES)

# Standard debug build. Assertions are checked and the GC is run before every allocation.
debug debug1::
	@mkdir -p out
	$(CC) $(CFLAGS) $(SGC) -o out/pyro $(FILES)

# As debug1, plus dumps bytecode and traces execution opcode-by-opcode.
debug2::
	@mkdir -p out
	$(CC) $(CFLAGS) $(SGC) $(TRACE) -o out/pyro $(FILES)

# As debug2, plus GC logging. Very verbose.
debug3::
	@mkdir -p out
	$(CC) $(CFLAGS) $(SGC) $(TRACE) $(LGC) -o out/pyro $(FILES)

# Runs the standard debug build, then runs the test suite.
check::
	@make debug
	./out/pyro test ./tests/*.pyro

clean::
	rm -f ./out/*
