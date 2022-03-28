# ----------- #
#  Variables  #
# ----------- #

CFLAGS = -Wall -Wextra --std=c11 --pedantic -Wno-unused-parameter -Wno-unused-function

DEBUG_1 = -D PYRO_DEBUG_STRESS_GC

DEBUG_2 = $(DEBUG_1) -D PYRO_DEBUG_DUMP_BYTECODE

DEBUG_3 = $(DEBUG_1) -D PYRO_DEBUG_TRACE_EXECUTION

DEBUG_4 = $(DEBUG_1) -D PYRO_DEBUG_DUMP_BYTECODE -D PYRO_DEBUG_TRACE_EXECUTION

DEBUG_5 = $(DEBUG_4) -D PYRO_DEBUG_LOG_GC

DEBUG_FLAGS = ${DEBUG_1}

SRC_FILES = src/vm/*.c src/std/*.c src/cli/*.c

LIB_FILES = src/lib/mt64/*.c src/lib/args/*.c src/lib/linenoise/*.c

OBJ_FILES = out/lib/sqlite.o

FILES = $(SRC_FILES) $(LIB_FILES) ${OBJ_FILES}

# --------------- #
#  Phony Targets  #
# --------------- #

# Optimized release build.
release:: ${OBJ_FILES}
	@mkdir -p out/release
	@printf "\e[1;32mBuilding\e[0m out/release/pyro\n"
	@$(CC) $(CFLAGS) -O3 -D NDEBUG -o out/release/pyro $(FILES) -lm -ldl -pthread
	@printf "\e[1;32m Version\e[0m " && ./out/release/pyro --version

# Unoptimized debug build.
debug:: ${OBJ_FILES}
	@mkdir -p out/debug
	@printf "\e[1;32mBuilding\e[0m out/debug/pyro\n"
	@$(CC) $(CFLAGS) -D DEBUG $(DEBUG_FLAGS) -o out/debug/pyro $(FILES) -lm -ldl -pthread
	@printf "\e[1;32m Version\e[0m " && ./out/debug/pyro --version

# Default debug build. Assertions are checked and the GC is run before every allocation.
debug1::
	@make debug DEBUG_FLAGS="${DEBUG_1}"

# Debug build. Dumps bytecode.
debug2::
	@make debug DEBUG_FLAGS="${DEBUG_2}"

# Debug build. Traces execution.
debug3::
	@make debug DEBUG_FLAGS="${DEBUG_3}"

# Debug build. Dumps bytecode and traces execution.
debug4::
	@make debug DEBUG_FLAGS="${DEBUG_4}"

# Debug build. Dumps bytecode, traces execution, and logs the GC.
debug5::
	@make debug DEBUG_FLAGS="${DEBUG_5}"

# Runs the standard debug build, then runs the test suite.
check::
	@make debug
	@printf "\e[1;32m Running\e[0m test suite\n\n"
	@./out/debug/pyro test ./tests/*.pyro

install::
	@printf "\e[1;32m Copying\e[0m out/release/pyro --> /usr/local/bin/pyro\n"
	@if [ -f .out/release/pyro ]; then cp .out/release/pyro /usr/local/bin/pyro; fi
	@if [ ! -f ./out/release/pyro ]; then printf "\e[1;31m   Error\e[0m the file out/release/pyro does not exist\n"; fi

clean::
	rm -rf ./out/*

# ----------- #
#  Libraries  #
# ----------- #

sqlite:: out/lib/sqlite.o

out/lib/sqlite.o:
	@mkdir -p out/lib
	@printf "\e[1;32mBuilding\e[0m sqlite\n"
	@$(CC) $(CFLAGS) -O3 -D NDEBUG -c src/lib/sqlite/sqlite3.c -o out/lib/sqlite.o
