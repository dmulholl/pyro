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

FILES = ${PYRO_FILES} ${LIB_FILES} -lm

# --------------- #
#  Phony Targets  #
# --------------- #

# Optimized release build.
release::
	@mkdir -p out/release
	@printf "\e[1;32mBuilding\e[0m release binary\n"
	@$(CC) $(CFLAGS) -O3 -D NDEBUG -o out/release/pyro $(FILES)
	@printf "\e[1;32m Version\e[0m " && ./out/release/pyro --version

# Debug build. Assertions are checked and the GC is run before every allocation.
debug1 debug::
	@mkdir -p out/debug
	@printf "\e[1;32mBuilding\e[0m debug binary\n"
	@$(CC) $(CFLAGS) $(STRESS_GC) -o out/debug/pyro $(FILES)
	@printf "\e[1;32m Version\e[0m " && ./out/debug/pyro --version

# Debug build. Dumps bytecode.
debug2::
	@mkdir -p out/debug
	@printf "\e[1;32mBuilding\e[0m debug binary (dumps bytecode)\n"
	@$(CC) $(CFLAGS) $(STRESS_GC) $(DUMP_CODE) -o out/debug/pyro $(FILES)

# Debug build. Traces execution.
debug3::
	@mkdir -p out/debug
	@printf "\e[1;32mBuilding\e[0m debug binary (traces execution)\n"
	@$(CC) $(CFLAGS) $(STRESS_GC) $(TRACE_EXEC) -o out/debug/pyro $(FILES)

# Debug build. Dumps bytecode and traces execution.
debug4::
	@mkdir -p out/debug
	@printf "\e[1;32mBuilding\e[0m debug binary (dumps bytecode, traces execution)\n"
	@$(CC) $(CFLAGS) $(STRESS_GC) $(DUMP_CODE) $(TRACE_EXEC) -o out/debug/pyro $(FILES)

# Debug build. Dumps bytecode, traces execution, and logs the GC.
debug5::
	@mkdir -p out/debug
	@printf "\e[1;32mBuilding\e[0m debug binary (dumps code, traces execution, logs GC)\n"
	@$(CC) $(CFLAGS) $(STRESS_GC) $(DUMP_CODE) $(TRACE_EXEC) $(LOG_GC) -o out/debug/pyro $(FILES)

# Runs the standard debug build, then runs the test suite.
check::
	@make debug
	@printf "\e[1;32m Running\e[0m test suite\n\n"
	@./out/debug/pyro test ./tests/*.pyro

clean::
	rm -rf ./out/*
