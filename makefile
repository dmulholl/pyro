# ----------- #
#  Variables  #
# ----------- #

CFLAGS = -Wall -Wextra --std=c11 --pedantic -fwrapv \
		 -Wno-unused-parameter -Wno-unused-function -Wno-unused-result

RELEASE_FLAGS = -rdynamic -O2 -D NDEBUG -D PYRO_VERSION_BUILD='"release"'
DEBUG_FLAGS = -rdynamic -D DEBUG -D PYRO_VERSION_BUILD='"debug"'

DEBUG_LEVEL_1 = -D PYRO_DEBUG_STRESS_GC
DEBUG_LEVEL_2 = $(DEBUG_LEVEL_1) -D PYRO_DEBUG_DUMP_BYTECODE
DEBUG_LEVEL_3 = $(DEBUG_LEVEL_2) -D PYRO_DEBUG_TRACE_EXECUTION
DEBUG_LEVEL_4 = $(DEBUG_LEVEL_3) -D PYRO_DEBUG_LOG_GC
DEBUG_LEVEL = $(DEBUG_LEVEL_1)

SRC_FILES = src/cli/*.c src/vm/*.c src/std/core/*.c src/std/c/*.c

OBJ_FILES = out/build/sqlite.o \
			out/build/bestline.o \
			out/build/args.o \
			out/build/mt64.o \
			out/build/std_mod_args.o \
			out/build/std_mod_email.o \
			out/build/std_mod_html.o \
			out/build/std_mod_cgi.o \
			out/build/std_mod_pretty.o \
			out/build/std_mod_json.o

INPUT = $(SRC_FILES) $(OBJ_FILES) -lm -ldl -pthread

# --------------- #
#  Phony Targets  #
# --------------- #

release: ## Builds the release binary.
release: $(OBJ_FILES)
	@mkdir -p out/release
	@printf "\e[1;32mBuilding\e[0m out/release/pyro\n"
	@$(CC) $(CFLAGS) $(RELEASE_FLAGS) -o out/release/pyro $(INPUT)
	@printf "\e[1;32m Version\e[0m " && ./out/release/pyro --version

debug: ## Builds the debug binary.
debug: $(OBJ_FILES)
	@mkdir -p out/debug
	@printf "\e[1;32mBuilding\e[0m out/debug/pyro\n"
	@$(CC) $(CFLAGS) $(DEBUG_FLAGS) $(DEBUG_LEVEL) -o out/debug/pyro $(INPUT)
	@printf "\e[1;32m Version\e[0m " && ./out/debug/pyro --version

debug1: ## Debug build. Checks assertions, stresses GC.
	@make debug DEBUG_LEVEL="$(DEBUG_LEVEL_1)"

debug2: ## As debug1, also dumps bytecode.
	@make debug DEBUG_LEVEL="$(DEBUG_LEVEL_2)"

debug3: ## As debug2, also traces execution.
	@make debug DEBUG_LEVEL="$(DEBUG_LEVEL_3)"

debug4: ## As debug3, also logs GC.
	@make debug DEBUG_LEVEL="$(DEBUG_LEVEL_4)"

check-debug: ## Builds the debug binary, then runs the test suite.
check-debug: tests/compiled_module.so
	@make debug
	@printf "\e[1;32m Running\e[0m test suite\n\n"
	@./out/debug/pyro test ./tests/*.pyro

check-release: ## Builds the release binary, then runs the test suite.
check-release: tests/compiled_module.so
	@make release
	@printf "\e[1;32m Running\e[0m test suite\n\n"
	@./out/release/pyro test ./tests/*.pyro

check:
	@make check-debug

install: ## Installs the release binary.
	@if [ -f ./out/release/pyro ]; then \
		printf "\e[1;32m Copying\e[0m out/release/pyro --> /usr/local/bin/pyro\n"; \
		rm -f /usr/local/bin/pyro; \
		cp ./out/release/pyro /usr/local/bin/pyro; \
	else \
		printf "\e[1;31m   Error\e[0m out/release/pyro not found\n"; \
		exit 1; \
	fi

clean: ## Deletes all build artifacts.
	rm -rf ./out/*
	rm -f ./tests/compiled_module.so

help: ## Prints available commands.
	@awk 'BEGIN {FS = ":.*?## "} /^[a-zA-Z0-9_-]+:.*?## / \
	{printf "\033[36m%-20s\033[0m %s\n", $$1, $$2}' $(MAKEFILE_LIST)

# ------------- #
#  C Libraries  #
# ------------- #

out/build/sqlite.o: lib/sqlite/sqlite3.c lib/sqlite/sqlite3.h
	@mkdir -p out/build
	@printf "\e[1;32mBuilding\e[0m sqlite\n"
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c lib/sqlite/sqlite3.c -o out/build/sqlite.o

out/build/bestline.o: lib/bestline/bestline.c lib/bestline/bestline.h
	@mkdir -p out/build
	@printf "\e[1;32mBuilding\e[0m bestline\n"
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c lib/bestline/bestline.c -o out/build/bestline.o

out/build/args.o: lib/args/args.c lib/args/args.h
	@mkdir -p out/build
	@printf "\e[1;32mBuilding\e[0m args\n"
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c lib/args/args.c -o out/build/args.o

out/build/mt64.o: lib/mt64/mt64.c lib/mt64/mt64.h
	@mkdir -p out/build
	@printf "\e[1;32mBuilding\e[0m mt64\n"
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c lib/mt64/mt64.c -o out/build/mt64.o

# ---------------- #
#  Pyro Libraries  #
# ---------------- #

# These are standard library modules written in Pyro. We use `xxd` to compile the source code
# into byte-arrays in C source file format, then embed them directly into the Pyro binary.

out/build/std_mod_args.o: src/std/pyro/std_mod_args.pyro
	@mkdir -p out/build
	@printf "\e[1;32mBuilding\e[0m \$$std:args\n"
	@cd src/std/pyro; xxd -i std_mod_args.pyro > ../../../out/build/std_mod_args.c
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c out/build/std_mod_args.c -o out/build/std_mod_args.o

out/build/std_mod_email.o: src/std/pyro/std_mod_email.pyro
	@mkdir -p out/build
	@printf "\e[1;32mBuilding\e[0m \$$std:email\n"
	@cd src/std/pyro; xxd -i std_mod_email.pyro > ../../../out/build/std_mod_email.c
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c out/build/std_mod_email.c -o out/build/std_mod_email.o

out/build/std_mod_html.o: src/std/pyro/std_mod_html.pyro
	@mkdir -p out/build
	@printf "\e[1;32mBuilding\e[0m \$$std:html\n"
	@cd src/std/pyro; xxd -i std_mod_html.pyro > ../../../out/build/std_mod_html.c
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c out/build/std_mod_html.c -o out/build/std_mod_html.o

out/build/std_mod_cgi.o: src/std/pyro/std_mod_cgi.pyro
	@mkdir -p out/build
	@printf "\e[1;32mBuilding\e[0m \$$std:cgi\n"
	@cd src/std/pyro; xxd -i std_mod_cgi.pyro > ../../../out/build/std_mod_cgi.c
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c out/build/std_mod_cgi.c -o out/build/std_mod_cgi.o

out/build/std_mod_json.o: src/std/pyro/std_mod_json.pyro
	@mkdir -p out/build
	@printf "\e[1;32mBuilding\e[0m \$$std:json\n"
	@cd src/std/pyro; xxd -i std_mod_json.pyro > ../../../out/build/std_mod_json.c
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c out/build/std_mod_json.c -o out/build/std_mod_json.o

out/build/std_mod_pretty.o: src/std/pyro/std_mod_pretty.pyro
	@mkdir -p out/build
	@printf "\e[1;32mBuilding\e[0m \$$std:pretty\n"
	@cd src/std/pyro; xxd -i std_mod_pretty.pyro > ../../../out/build/std_mod_pretty.c
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c out/build/std_mod_pretty.c -o out/build/std_mod_pretty.o

# ---------------------- #
#  Test Compiled Module  #
# ---------------------- #

tests/compiled_module.so: tests/compiled_module.c
	@printf "\e[1;32mBuilding\e[0m tests/compiled_module.so\n"
	@${CC} ${CFLAGS} -O2 -D NDEBUG -shared -fPIC -undefineddynamic_lookup -o tests/compiled_module.so tests/compiled_module.c

check-compiled-module:
	@rm -f ./tests/compiled_module.so
	@make tests/compiled_module.so
	@./out/debug/pyro test -v ./tests/import_compiled_module.pyro
