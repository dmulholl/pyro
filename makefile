# ----------- #
#  Variables  #
# ----------- #

CFLAGS = -Wall -Wextra --std=c11 --pedantic -Wno-unused-parameter -Wno-unused-function

RELEASE_FLAGS = -O3 -D NDEBUG -D PYRO_VERSION_BUILD='"release"'
DEBUG_FLAGS = -D DEBUG -D PYRO_VERSION_BUILD='"debug"'

DEBUG_LEVEL_1 = -D PYRO_DEBUG_STRESS_GC
DEBUG_LEVEL_2 = -D PYRO_DEBUG_STRESS_GC -D PYRO_DEBUG_DUMP_BYTECODE
DEBUG_LEVEL_3 = -D PYRO_DEBUG_STRESS_GC -D PYRO_DEBUG_DUMP_BYTECODE -D PYRO_DEBUG_TRACE_EXECUTION
DEBUG_LEVEL_4 = -D PYRO_DEBUG_STRESS_GC -D PYRO_DEBUG_DUMP_BYTECODE -D PYRO_DEBUG_TRACE_EXECUTION \
				-D PYRO_DEBUG_LOG_GC

DEBUG_LEVEL = $(DEBUG_LEVEL_1)

SRC_FILES = src/vm/*.c src/std/core/*.c src/std/modules/*.c src/cli/*.c
LIB_FILES = src/lib/mt64/*.c src/lib/args/*.c src/lib/linenoise/*.c
OBJ_FILES = out/lib/sqlite.o out/lib/lib_args.o

FILES = $(SRC_FILES) $(LIB_FILES) ${OBJ_FILES} -lm -ldl -pthread

# --------------- #
#  Phony Targets  #
# --------------- #

release: ## Builds the release binary.
release: ${OBJ_FILES}
	@mkdir -p out/release
	@printf "\e[1;32mBuilding\e[0m out/release/pyro\n"
	@$(CC) $(CFLAGS) $(RELEASE_FLAGS) -o out/release/pyro $(FILES)
	@printf "\e[1;32m Version\e[0m " && ./out/release/pyro --version

debug: ## Builds the debug binary.
debug: ${OBJ_FILES}
	@mkdir -p out/debug
	@printf "\e[1;32mBuilding\e[0m out/debug/pyro\n"
	@$(CC) $(CFLAGS) $(DEBUG_FLAGS) $(DEBUG_LEVEL) -o out/debug/pyro $(FILES)
	@printf "\e[1;32m Version\e[0m " && ./out/debug/pyro --version

debug1: ## Checks assertions, stresses GC.
debug1:
	@make debug DEBUG_LEVEL="${DEBUG_LEVEL_1}"

debug2: ## Dumps bytecode.
debug2:
	@make debug DEBUG_LEVEL="${DEBUG_LEVEL_2}"

debug3: ## Traces execution.
debug3:
	@make debug DEBUG_LEVEL="${DEBUG_LEVEL_3}"

debug4: ## Logs GC.
debug4:
	@make debug DEBUG_LEVEL="${DEBUG_LEVEL_4}"

check: ## Builds the debug binary, then runs the test suite.
check:
	@make debug
	@printf "\e[1;32m Running\e[0m test suite\n\n"
	@./out/debug/pyro test ./tests/*.pyro

install: ## Builds and installs the release binary.
install:
	@if [ ! -f ./out/release/pyro ]; then make release; fi
	@if [ -f ./out/release/pyro ]; then printf "\e[1;32m Copying\e[0m out/release/pyro --> /usr/local/bin/pyro\n"; fi
	@if [ -f ./out/release/pyro ]; then cp ./out/release/pyro /usr/local/bin/pyro; fi

clean: ## Deletes all build artifacts.
clean:
	rm -rf ./out/*
	rm -f ./src/std/pyro/*.c

help:
	@awk 'BEGIN {FS = ":.*?## "} /^[a-zA-Z0-9_-]+:.*?## / \
	{printf "\033[36m%-20s\033[0m %s\n", $$1, $$2}' $(MAKEFILE_LIST)

sqlite: out/lib/sqlite.o

# ----------- #
#  Libraries  #
# ----------- #

out/lib/sqlite.o:
	@mkdir -p out/lib
	@printf "\e[1;32mBuilding\e[0m sqlite\n"
	@$(CC) $(CFLAGS) -O3 -D NDEBUG -c src/lib/sqlite/sqlite3.c -o out/lib/sqlite.o

out/lib/lib_args.o: src/std/pyro/lib_args.pyro
	@mkdir -p out/lib
	@printf "\e[1;32mBuilding\e[0m \$$std:args\n"
	@cd src/std/pyro; xxd -i lib_args.pyro > lib_args.c
	@$(CC) $(CFLAGS) -O3 -D NDEBUG -c src/std/pyro/lib_args.c -o out/lib/lib_args.o
