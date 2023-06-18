# ----------- #
#  Variables  #
# ----------- #

# Common flags for both release and debug builds.
CFLAGS = -Wall -Wextra --std=c11 --pedantic -fwrapv \
		 -Wno-unused-parameter \
		 -Wno-unused-function \
		 -Wno-unused-result

# Extra flags for release builds.
RELEASE_FLAGS = -rdynamic -D PYRO_VERSION_BUILD='"release"' -D NDEBUG -O3

# Extra flags for debug builds.
DEBUG_FLAGS = -rdynamic -D PYRO_VERSION_BUILD='"debug"' -D PYRO_DEBUG -D PYRO_DEBUG_STRESS_GC

# Core files for the Pyro VM.
HDR_FILES = inc/*.h
SRC_FILES = src/*.c std/builtins/*.c std/modules/*.c
OBJ_FILES = build/common/mt64.o \
			build/common/embeds.o

# Files for the CLI binary.
CLI_HDR_FILES = cli/*.h
CLI_SRC_FILES = cli/*.c
CLI_OBJ_FILES = build/common/bestline.o \
				build/common/args.o

# Files for baked-application binaries.
APP_SRC_FILES = app/*.c

# Default name for the baked-application binary.
APPNAME ?= app

# --------------- #
#  Build Targets  #
# --------------- #

all: ## Builds both the release and debug binaries.
	@make debug
	@make release

release: ## Builds the release binary.
release: $(OBJ_FILES) $(CLI_OBJ_FILES)
	@mkdir -p build/release
	@printf "\e[1;32mBuilding\e[0m build/release/pyro\n"
	@$(CC) $(CFLAGS) $(RELEASE_FLAGS) \
		-o build/release/pyro \
		$(SRC_FILES) $(OBJ_FILES) \
		$(CLI_SRC_FILES) $(CLI_OBJ_FILES) \
		-lm -ldl -pthread
	@printf "\e[1;32m Version\e[0m " && ./build/release/pyro --version

debug: ## Builds the debug binary.
debug: $(OBJ_FILES) $(CLI_OBJ_FILES)
	@mkdir -p build/debug
	@printf "\e[1;32mBuilding\e[0m build/debug/pyro\n"
	@$(CC) $(CFLAGS) $(DEBUG_FLAGS) \
		-o build/debug/pyro \
		$(SRC_FILES) $(OBJ_FILES) \
		$(CLI_SRC_FILES) $(CLI_OBJ_FILES) \
		-lm -ldl -pthread
	@printf "\e[1;32m Version\e[0m " && ./build/debug/pyro --version

debug-sanitize: ## Builds a debug binary with sanitizer checks.
debug-sanitize: $(OBJ_FILES) $(CLI_OBJ_FILES)
	@mkdir -p build/debug
	@printf "\e[1;32mBuilding\e[0m build/debug/pyro\n"
	@$(CC) $(CFLAGS) $(DEBUG_FLAGS) \
		-fsanitize=address,undefined \
		-o build/debug/pyro \
		$(SRC_FILES) $(OBJ_FILES) \
		$(CLI_SRC_FILES) $(CLI_OBJ_FILES) \
		-lm -ldl -pthread
	@printf "\e[1;32m Version\e[0m " && ./build/debug/pyro --version

debug-dump: ## Builds a debug binary that also dumps bytecode.
debug-dump: $(OBJ_FILES) $(CLI_OBJ_FILES)
	@mkdir -p build/debug
	@printf "\e[1;32mBuilding\e[0m build/debug/pyro\n"
	@$(CC) $(CFLAGS) $(DEBUG_FLAGS) -D PYRO_DEBUG_DUMP_BYTECODE \
		-o build/debug/pyro \
		$(SRC_FILES) $(OBJ_FILES) \
		$(CLI_SRC_FILES) $(CLI_OBJ_FILES) \
		-lm -ldl -pthread
	@printf "\e[1;32m Version\e[0m " && ./build/debug/pyro --version

debug-trace: ## Builds a debug binary that also dumps bytecode and traces execution.
debug-trace: $(OBJ_FILES) $(CLI_OBJ_FILES)
	@mkdir -p build/debug
	@printf "\e[1;32mBuilding\e[0m build/debug/pyro\n"
	@$(CC) $(CFLAGS) $(DEBUG_FLAGS) -D PYRO_DEBUG_DUMP_BYTECODE -D PYRO_DEBUG_TRACE_EXECUTION \
		-o build/debug/pyro \
		$(SRC_FILES) $(OBJ_FILES) \
		$(CLI_SRC_FILES) $(CLI_OBJ_FILES) \
		-lm -ldl -pthread
	@printf "\e[1;32m Version\e[0m " && ./build/debug/pyro --version

.PHONY: app
app: ## Builds a baked-application binary.
app: $(OBJ_FILES)
	@mkdir -p build/release
	@printf "\e[1;32mBuilding\e[0m build/release/$(APPNAME)\n"
	@$(CC) $(CFLAGS) $(RELEASE_FLAGS) \
		-o build/release/$(APPNAME) \
		$(SRC_FILES) $(OBJ_FILES) \
		$(APP_SRC_FILES) \
		-lm -ldl -pthread

# ------------------ #
#  Utility Commands  #
# ------------------ #

check-release: ## Builds the release binary, then runs the test suite.
check-release: tests/compiled_module.so
	@make release
	@printf "\e[1;32m Running\e[0m build/release/pyro test tests/*.pyro\n\n"
	@./build/release/pyro test ./tests/*.pyro

check-debug: ## Builds the debug binary, then runs the test suite.
check-debug: tests/compiled_module.so
	@make debug
	@printf "\e[1;32m Running\e[0m build/debug/pyro test tests/*.pyro\n\n"
	@./build/debug/pyro test ./tests/*.pyro

check-sanitize: ## Builds the debug binary with sanitizer checks, then runs the test suite.
check-sanitize: tests/compiled_module.so
	@make debug-sanitize
	@printf "\e[1;32m Running\e[0m build/debug/pyro test tests/*.pyro\n\n"
	@./build/debug/pyro test ./tests/*.pyro

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

# ----------------------- #
#  Third-Party Libraries  #
# ----------------------- #

build/common/sqlite.o: lib/sqlite/sqlite3.c lib/sqlite/sqlite3.h
	@mkdir -p build/common
	@printf "\e[1;32mBuilding\e[0m build/common/sqlite.o\n"
	@$(CC) $(CFLAGS) -O3 -D NDEBUG -c lib/sqlite/sqlite3.c -o build/common/sqlite.o

build/common/bestline.o: lib/bestline/bestline.c lib/bestline/bestline.h
	@mkdir -p build/common
	@printf "\e[1;32mBuilding\e[0m build/common/bestline.o\n"
	@$(CC) $(CFLAGS) -O3 -D NDEBUG -c lib/bestline/bestline.c -o build/common/bestline.o

build/common/args.o: lib/args/args.c lib/args/args.h
	@mkdir -p build/common
	@printf "\e[1;32mBuilding\e[0m build/common/args.o\n"
	@$(CC) $(CFLAGS) -O3 -D NDEBUG -c lib/args/args.c -o build/common/args.o

build/common/mt64.o: lib/mt64/mt64.c lib/mt64/mt64.h
	@mkdir -p build/common
	@printf "\e[1;32mBuilding\e[0m build/common/mt64.o\n"
	@$(CC) $(CFLAGS) -O3 -D NDEBUG -c lib/mt64/mt64.c -o build/common/mt64.o

# ---------------- #
#  Embedded Files  #
# ---------------- #

build/common/embeds.o: build/common/embeds.c
	@mkdir -p build/common
	@printf "\e[1;32mBuilding\e[0m build/common/embeds.o\n"
	@$(CC) $(CFLAGS) -O3 -D NDEBUG -c build/common/embeds.c -o build/common/embeds.o

build/common/embeds.c: build/bin/embed $(shell find ./embed -type f)
	@mkdir -p build/common
	@printf "\e[1;32mBuilding\e[0m build/common/embeds.c\n"
	@build/bin/embed ./embed > build/common/embeds.c

# ------------------ #
#  Utility Binaries  #
# ------------------ #

build/bin/embed: tools/embed.c
	@mkdir -p build/bin
	@$(CC) $(CFLAGS) -O3 -D NDEBUG tools/embed.c -o build/bin/embed

# ---------------------- #
#  Test Compiled Module  #
# ---------------------- #

tests/compiled_module.so: tests/compiled_module.c inc/*.h
	@printf "\e[1;32mBuilding\e[0m tests/compiled_module.so\n"
	@$(CC) $(CFLAGS) -O3 -D NDEBUG -shared -fPIC -Wl,-undefined,dynamic_lookup -o tests/compiled_module.so tests/compiled_module.c

check-compiled-module:
	@rm -f ./tests/compiled_module.so
	@make tests/compiled_module.so
	@./build/debug/pyro test -v ./tests/import_compiled_module.pyro
