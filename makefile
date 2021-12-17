# ----------- #
#  Variables  #
# ----------- #

CFLAGS = -Wall -Wextra --std=c11 --pedantic -Wno-unused-parameter -Wno-unused-function

STRESS_GC = -D PYRO_DEBUG_STRESS_GC

LOG_GC = -D PYRO_DEBUG_LOG_GC

DUMP_CODE = -D PYRO_DEBUG_DUMP_BYTECODE

TRACE_EXEC = -D PYRO_DEBUG_TRACE_EXECUTION

PYRO_FILES = src/vm/*.c src/std/*.c src/cli/*.c

LIB_FILES = src/lib/mt64/*.c src/lib/args/*.c src/lib/linenoise/*.c src/lib/os/*.c

FILES = ${PYRO_FILES} ${LIB_FILES}

# --------------- #
#  Phony Targets  #
# --------------- #

all:
	@echo "Building debug binary..."
	@make debug
	@echo "Building release binary..."
	@make release

# Optimized release build.
release::
	@mkdir -p out/release
	$(CC) $(CFLAGS) -O3 -D NDEBUG -o out/release/pyro $(FILES)

# Basic debug build. Assertions are checked and the GC is run before every allocation.
debug debug1::
	@mkdir -p out/debug
	$(CC) $(CFLAGS) $(STRESS_GC) -o out/debug/pyro $(FILES)

# As debug1, also dumps bytecode and traces execution opcode-by-opcode.
debug2::
	@mkdir -p out/debug
	$(CC) $(CFLAGS) $(STRESS_GC) $(DUMP_CODE) $(TRACE_EXEC) -o out/debug/pyro $(FILES)

# As debug2, with GC logging. This generates *very* verbose output.
debug3::
	@mkdir -p out/debug
	$(CC) $(CFLAGS) $(STRESS_GC) $(DUMP_CODE) $(TRACE_EXEC) $(LOG_GC) -o out/debug/pyro $(FILES)

# Runs the standard debug build, then runs the test suite.
check::
	@make debug
	./out/debug/pyro test ./tests/*.pyro

clean::
	rm -rf ./out/*
