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

release::
	@mkdir -p out
	$(CC) $(CFLAGS) -D NDEBUG -o out/pyro $(FILES)

debug::
	@mkdir -p out
	$(CC) $(CFLAGS) $(SGC) -o out/pyro $(FILES)

trace::
	@mkdir -p out
	$(CC) $(CFLAGS) $(SGC) $(TRACE) -o out/pyro $(FILES)

log::
	@mkdir -p out
	$(CC) $(CFLAGS) $(SGC) $(TRACE) $(LGC) -o out/pyro $(FILES)

check::
	@make debug
	./out/pyro test ./tests/*.pyro

clean::
	rm -f ./out/*
