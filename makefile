# ----------- #
#  Variables  #
# ----------- #

CFLAGS = -Wall -Wextra --std=c11 --pedantic -fwrapv \
		 -Wno-unused-parameter \
		 -Wno-unused-function \
		 -Wno-unused-result

RELEASE_FLAGS = -rdynamic -D PYRO_VERSION_BUILD='"release"' -D NDEBUG -O2
DEBUG_FLAGS_0 = -rdynamic -D PYRO_VERSION_BUILD='"debug"' -D PYRO_DEBUG
DEBUG_FLAGS_1 = $(DEBUG_FLAGS_0) -D PYRO_DEBUG_STRESS_GC
DEBUG_FLAGS_2 = $(DEBUG_FLAGS_1) -D PYRO_DEBUG_DUMP_BYTECODE
DEBUG_FLAGS_3 = $(DEBUG_FLAGS_2) -D PYRO_DEBUG_TRACE_EXECUTION
DEBUG_FLAGS_4 = $(DEBUG_FLAGS_3) -D PYRO_DEBUG_LOG_GC
DEBUG_FLAGS = $(DEBUG_FLAGS_1)

HDR_FILES = cli/*.h inc/*.h
SRC_FILES = cli/*.c src/*.c std/builtins/*.c std/mods/c/*.c

OBJ_FILES = out/build/sqlite.o \
			out/build/bestline.o \
			out/build/args.o \
			out/build/mt64.o \
			out/build/std_mod_args.o \
			out/build/std_mod_email.o \
			out/build/std_mod_html.o \
			out/build/std_mod_cgi.o \
			out/build/std_mod_pretty.o \
			out/build/std_mod_url.o \
			out/build/std_mod_json.o

# --------------- #
#  Phony Targets  #
# --------------- #

release: ## Builds the release binary.
release: $(HDR_FILES) $(SRC_FILES) $(OBJ_FILES)
	@mkdir -p out/release
	@printf "\e[1;32mBuilding\e[0m out/release/pyro\n"
	@$(CC) $(CFLAGS) $(RELEASE_FLAGS) -o out/release/pyro $(SRC_FILES) $(OBJ_FILES) -lm -ldl -pthread
	@printf "\e[1;32m Version\e[0m " && ./out/release/pyro --version

debug: ## Builds the debug binary.
debug: $(HDR_FILES) $(SRC_FILES) $(OBJ_FILES)
	@mkdir -p out/debug
	@printf "\e[1;32mBuilding\e[0m out/debug/pyro\n"
	@$(CC) $(CFLAGS) $(DEBUG_FLAGS) -o out/debug/pyro $(SRC_FILES) $(OBJ_FILES) -lm -ldl -pthread
	@printf "\e[1;32m Version\e[0m " && ./out/debug/pyro --version

debug1: ## Default debug level. Checks assertions, stresses the garbage collector.
	@make debug DEBUG_FLAGS="$(DEBUG_FLAGS_1)"

debug2: ## As debug1, also dumps bytecode.
	@make debug DEBUG_FLAGS="$(DEBUG_FLAGS_2)"

debug3: ## As debug2, also traces execution.
	@make debug DEBUG_FLAGS="$(DEBUG_FLAGS_3)"

debug4: ## As debug3, also logs garbage collection.
	@make debug DEBUG_FLAGS="$(DEBUG_FLAGS_4)"

check-debug: ## Builds the debug binary, then runs the test suite.
check-debug: tests/compiled_module.so
	@make debug
	@printf "\e[1;32m Running\e[0m out/debug/pyro test tests/*.pyro\n\n"
	@./out/debug/pyro test ./tests/*.pyro

check-release: ## Builds the release binary, then runs the test suite.
check-release: tests/compiled_module.so
	@make release
	@printf "\e[1;32m Running\e[0m out/release/pyro test tests/*.pyro\n\n"
	@./out/release/pyro test ./tests/*.pyro

check: ## Alias for check-debug.
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

out/build/std_mod_args.o: std/mods/pyro/std_mod_args.pyro
	@mkdir -p out/build
	@printf "\e[1;32mBuilding\e[0m std::args\n"
	@cd std/mods/pyro; xxd -i std_mod_args.pyro > ../../../out/build/std_mod_args.c
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c out/build/std_mod_args.c -o out/build/std_mod_args.o

out/build/std_mod_email.o: std/mods/pyro/std_mod_email.pyro
	@mkdir -p out/build
	@printf "\e[1;32mBuilding\e[0m std::email\n"
	@cd std/mods/pyro; xxd -i std_mod_email.pyro > ../../../out/build/std_mod_email.c
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c out/build/std_mod_email.c -o out/build/std_mod_email.o

out/build/std_mod_html.o: std/mods/pyro/std_mod_html.pyro
	@mkdir -p out/build
	@printf "\e[1;32mBuilding\e[0m std::html\n"
	@cd std/mods/pyro; xxd -i std_mod_html.pyro > ../../../out/build/std_mod_html.c
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c out/build/std_mod_html.c -o out/build/std_mod_html.o

out/build/std_mod_cgi.o: std/mods/pyro/std_mod_cgi.pyro
	@mkdir -p out/build
	@printf "\e[1;32mBuilding\e[0m std::cgi\n"
	@cd std/mods/pyro; xxd -i std_mod_cgi.pyro > ../../../out/build/std_mod_cgi.c
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c out/build/std_mod_cgi.c -o out/build/std_mod_cgi.o

out/build/std_mod_json.o: std/mods/pyro/std_mod_json.pyro
	@mkdir -p out/build
	@printf "\e[1;32mBuilding\e[0m std::json\n"
	@cd std/mods/pyro; xxd -i std_mod_json.pyro > ../../../out/build/std_mod_json.c
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c out/build/std_mod_json.c -o out/build/std_mod_json.o

out/build/std_mod_pretty.o: std/mods/pyro/std_mod_pretty.pyro
	@mkdir -p out/build
	@printf "\e[1;32mBuilding\e[0m std::pretty\n"
	@cd std/mods/pyro; xxd -i std_mod_pretty.pyro > ../../../out/build/std_mod_pretty.c
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c out/build/std_mod_pretty.c -o out/build/std_mod_pretty.o

out/build/std_mod_url.o: std/mods/pyro/std_mod_url.pyro
	@mkdir -p out/build
	@printf "\e[1;32mBuilding\e[0m std::url\n"
	@cd std/mods/pyro; xxd -i std_mod_url.pyro > ../../../out/build/std_mod_url.c
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c out/build/std_mod_url.c -o out/build/std_mod_url.o

# ---------------------- #
#  Test Compiled Module  #
# ---------------------- #

tests/compiled_module.so: tests/compiled_module.c inc/*.h
	@printf "\e[1;32mBuilding\e[0m tests/compiled_module.so\n"
	@${CC} ${CFLAGS} -O2 -D NDEBUG -shared -fPIC -Wl,-undefined,dynamic_lookup -o tests/compiled_module.so tests/compiled_module.c

check-compiled-module:
	@rm -f ./tests/compiled_module.so
	@make tests/compiled_module.so
	@./out/debug/pyro test -v ./tests/import_compiled_module.pyro
