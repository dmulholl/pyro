#ifndef pyro_prng_h
#define pyro_prng_h

typedef struct {
    uint64_t s;
} pyro_splitmix64_state_t;

uint64_t pyro_splitmix64_next(pyro_splitmix64_state_t* state);

typedef struct {
    uint64_t s[4];
} pyro_xoshiro256ss_state_t;

void pyro_xoshiro256ss_init(pyro_xoshiro256ss_state_t* state, uint64_t seed);
uint64_t pyro_xoshiro256ss_next(pyro_xoshiro256ss_state_t* state);
uint64_t pyro_xoshiro256ss_next_in_range(pyro_xoshiro256ss_state_t* state, uint64_t n);

#endif
