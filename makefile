# ----------- #
#  Variables  #
# ----------- #

# Common flags for both the release and debug builds.
CFLAGS = -Wall -Wextra --std=c11 --pedantic -fwrapv \
		 -Wno-unused-parameter \
		 -Wno-unused-function \
		 -Wno-unused-result

RELEASE_FLAGS = -rdynamic -D PYRO_VERSION_BUILD='"release"' -D NDEBUG -O2
DEBUG_FLAGS   = -rdynamic -D PYRO_VERSION_BUILD='"debug"' -D PYRO_DEBUG -D PYRO_DEBUG_STRESS_GC

HDR_FILES = cli/*.h inc/*.h
SRC_FILES = cli/*.c src/*.c std/builtins/*.c std/mods/c/*.c

OBJ_FILES = build/common/sqlite.o \
			build/common/bestline.o \
			build/common/args.o \
			build/common/mt64.o \
			build/common/std_mod_args.o \
			build/common/std_mod_sendmail.o \
			build/common/std_mod_html.o \
			build/common/std_mod_cgi.o \
			build/common/std_mod_pretty.o \
			build/common/std_mod_url.o \
			build/common/std_mod_json.o

# --------------- #
#  Phony Targets  #
# --------------- #

all: ## Builds both the release and debug binaries.
	@make debug
	@make release

release: ## Builds the release binary.
release: $(HDR_FILES) $(SRC_FILES) $(OBJ_FILES)
	@mkdir -p build/release
	@printf "\e[1;32mBuilding\e[0m build/release/pyro\n"
	@$(CC) $(CFLAGS) $(RELEASE_FLAGS) \
		-o build/release/pyro $(SRC_FILES) $(OBJ_FILES) -lm -ldl -pthread
	@printf "\e[1;32m Version\e[0m " && ./build/release/pyro --version

debug: ## Builds the debug binary.
debug: $(HDR_FILES) $(SRC_FILES) $(OBJ_FILES)
	@mkdir -p build/debug
	@printf "\e[1;32mBuilding\e[0m build/debug/pyro\n"
	@$(CC) $(CFLAGS) $(DEBUG_FLAGS) \
		-o build/debug/pyro $(SRC_FILES) $(OBJ_FILES) -lm -ldl -pthread
	@printf "\e[1;32m Version\e[0m " && ./build/debug/pyro --version

debug-sanitize: ## Builds a debug binary with sanitizer checks.
debug-sanitize: $(HDR_FILES) $(SRC_FILES) $(OBJ_FILES)
	@mkdir -p build/debug
	@printf "\e[1;32mBuilding\e[0m build/debug/pyro\n"
	@$(CC) $(CFLAGS) $(DEBUG_FLAGS) -fsanitize=address,undefined \
		-o build/debug/pyro $(SRC_FILES) $(OBJ_FILES) -lm -ldl -pthread
	@printf "\e[1;32m Version\e[0m " && ./build/debug/pyro --version

debug-dump: ## Builds a debug binary that also dumps bytecode.
debug-dump: $(HDR_FILES) $(SRC_FILES) $(OBJ_FILES)
	@mkdir -p build/debug
	@printf "\e[1;32mBuilding\e[0m build/debug/pyro\n"
	@$(CC) $(CFLAGS) $(DEBUG_FLAGS) -D PYRO_DEBUG_DUMP_BYTECODE \
		-o build/debug/pyro $(SRC_FILES) $(OBJ_FILES) -lm -ldl -pthread
	@printf "\e[1;32m Version\e[0m " && ./build/debug/pyro --version

debug-trace: ## Builds a debug binary that also dumps bytecode and traces execution.
debug-trace: $(HDR_FILES) $(SRC_FILES) $(OBJ_FILES)
	@mkdir -p build/debug
	@printf "\e[1;32mBuilding\e[0m build/debug/pyro\n"
	@$(CC) $(CFLAGS) $(DEBUG_FLAGS) -D PYRO_DEBUG_DUMP_BYTECODE -D PYRO_DEBUG_TRACE_EXECUTION \
		-o build/debug/pyro $(SRC_FILES) $(OBJ_FILES) -lm -ldl -pthread
	@printf "\e[1;32m Version\e[0m " && ./build/debug/pyro --version

check-debug: ## Builds the debug binary, then runs the test suite.
check-debug: tests/compiled_module.so
	@make debug
	@printf "\e[1;32m Running\e[0m build/debug/pyro test tests/*.pyro\n\n"
	@./build/debug/pyro test ./tests/*.pyro

check-release: ## Builds the release binary, then runs the test suite.
check-release: tests/compiled_module.so
	@make release
	@printf "\e[1;32m Running\e[0m build/release/pyro test tests/*.pyro\n\n"
	@./build/release/pyro test ./tests/*.pyro

check: ## Alias for check-debug.
	@make check-debug

install: ## Installs the release binary.
	@if [ -f ./build/release/pyro ]; then \
		printf "\e[1;32m Copying\e[0m build/release/pyro --> /usr/local/bin/pyro\n"; \
		rm -f /usr/local/bin/pyro; \
		cp -f ./build/release/pyro /usr/local/bin/pyro; \
	else \
		printf "\e[1;31m   Error\e[0m build/release/pyro not found\n"; \
		exit 1; \
	fi

clean: ## Deletes all build artifacts.
	rm -rf ./build/*
	rm -f ./tests/compiled_module.so

help: ## Prints available commands.
	@awk 'BEGIN {FS = ":.*?## "} /^[a-zA-Z0-9_-]+:.*?## / \
	{printf "\033[36m%-20s\033[0m %s\n", $$1, $$2}' $(MAKEFILE_LIST)

# ------------- #
#  C Libraries  #
# ------------- #

build/common/sqlite.o: lib/sqlite/sqlite3.c lib/sqlite/sqlite3.h
	@mkdir -p build/common
	@printf "\e[1;32mBuilding\e[0m sqlite\n"
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c lib/sqlite/sqlite3.c -o build/common/sqlite.o

build/common/bestline.o: lib/bestline/bestline.c lib/bestline/bestline.h
	@mkdir -p build/common
	@printf "\e[1;32mBuilding\e[0m bestline\n"
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c lib/bestline/bestline.c -o build/common/bestline.o

build/common/args.o: lib/args/args.c lib/args/args.h
	@mkdir -p build/common
	@printf "\e[1;32mBuilding\e[0m args\n"
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c lib/args/args.c -o build/common/args.o

build/common/mt64.o: lib/mt64/mt64.c lib/mt64/mt64.h
	@mkdir -p build/common
	@printf "\e[1;32mBuilding\e[0m mt64\n"
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c lib/mt64/mt64.c -o build/common/mt64.o

# ---------------- #
#  Pyro Libraries  #
# ---------------- #

# These are standard library modules written in Pyro. We use `text2array` to compile the source
# code into byte-arrays in C source file format, then embed them directly into the Pyro binary.

build/common/std_mod_args.o: build/bin/text2array std/mods/pyro/args.pyro
	@mkdir -p build/common
	@printf "\e[1;32mBuilding\e[0m std::args\n"
	@build/bin/text2array pyro_mod_args < std/mods/pyro/args.pyro > build/common/std_mod_args.c
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c build/common/std_mod_args.c -o build/common/std_mod_args.o

build/common/std_mod_sendmail.o: build/bin/text2array std/mods/pyro/sendmail.pyro
	@mkdir -p build/common
	@printf "\e[1;32mBuilding\e[0m std::sendmail\n"
	@build/bin/text2array pyro_mod_sendmail < std/mods/pyro/sendmail.pyro > build/common/std_mod_sendmail.c
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c build/common/std_mod_sendmail.c -o build/common/std_mod_sendmail.o

build/common/std_mod_html.o: build/bin/text2array std/mods/pyro/html.pyro
	@mkdir -p build/common
	@printf "\e[1;32mBuilding\e[0m std::html\n"
	@build/bin/text2array pyro_mod_html < std/mods/pyro/html.pyro > build/common/std_mod_html.c
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c build/common/std_mod_html.c -o build/common/std_mod_html.o

build/common/std_mod_cgi.o: build/bin/text2array std/mods/pyro/cgi.pyro
	@mkdir -p build/common
	@printf "\e[1;32mBuilding\e[0m std::cgi\n"
	@build/bin/text2array pyro_mod_cgi < std/mods/pyro/cgi.pyro > build/common/std_mod_cgi.c
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c build/common/std_mod_cgi.c -o build/common/std_mod_cgi.o

build/common/std_mod_json.o: build/bin/text2array std/mods/pyro/json.pyro
	@mkdir -p build/common
	@printf "\e[1;32mBuilding\e[0m std::json\n"
	@build/bin/text2array pyro_mod_json < std/mods/pyro/json.pyro > build/common/std_mod_json.c
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c build/common/std_mod_json.c -o build/common/std_mod_json.o

build/common/std_mod_pretty.o: build/bin/text2array std/mods/pyro/pretty.pyro
	@mkdir -p build/common
	@printf "\e[1;32mBuilding\e[0m std::pretty\n"
	@build/bin/text2array pyro_mod_pretty < std/mods/pyro/pretty.pyro > build/common/std_mod_pretty.c
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c build/common/std_mod_pretty.c -o build/common/std_mod_pretty.o

build/common/std_mod_url.o: build/bin/text2array std/mods/pyro/url.pyro
	@mkdir -p build/common
	@printf "\e[1;32mBuilding\e[0m std::url\n"
	@build/bin/text2array pyro_mod_url < std/mods/pyro/url.pyro > build/common/std_mod_url.c
	@$(CC) $(CFLAGS) -O2 -D NDEBUG -c build/common/std_mod_url.c -o build/common/std_mod_url.o

# ------------------ #
#  Utility Binaries  #
# ------------------ #

build/bin/text2array: etc/text2array.c
	@mkdir -p build/bin
	@$(CC) $(CFLAGS) -O2 -D NDEBUG etc/text2array.c -o build/bin/text2array

# ---------------------- #
#  Test Compiled Module  #
# ---------------------- #

tests/compiled_module.so: tests/compiled_module.c inc/*.h
	@printf "\e[1;32mBuilding\e[0m tests/compiled_module.so\n"
	@${CC} ${CFLAGS} -O2 -D NDEBUG -shared -fPIC -Wl,-undefined,dynamic_lookup -o tests/compiled_module.so tests/compiled_module.c

check-compiled-module:
	@rm -f ./tests/compiled_module.so
	@make tests/compiled_module.so
	@./build/debug/pyro test -v ./tests/import_compiled_module.pyro
