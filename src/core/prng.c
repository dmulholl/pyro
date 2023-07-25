#include "../includes/pyro.h"

// -------------------------------------------------------------------------
// xoshiro256** 1.0
//
// Adapted from: https://prng.di.unimi.it/xoshiro256starstar.c
//
// The original source code contained the following license declaration:
//
// Written in 2018 by David Blackman and Sebastiano Vigna (vigna@acm.org)
//
// To the extent possible under law, the author has dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
//
// See <http://creativecommons.org/publicdomain/zero/1.0/>.
// -------------------------------------------------------------------------

static inline uint64_t rotate(uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

uint64_t pyro_xoshiro256ss_next(pyro_xoshiro256ss_state_t* state) {
    uint64_t* s = state->s;

    const uint64_t result = rotate(s[1] * 5, 7) * 9;
    const uint64_t t = s[1] << 17;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];

    s[2] ^= t;
    s[3] = rotate(s[3], 45);

    return result;
}

// -------------------------------------------------------------------------
// END xoshiro256**
// -------------------------------------------------------------------------


// -------------------------------------------------------------------------
// SplitMix64
//
// Adapted from: https://prng.di.unimi.it/splitmix64.c
//
// The original source code contained the following license declaration:
//
// Written in 2015 by Sebastiano Vigna (vigna@acm.org)
//
// To the extent possible under law, the author has dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
//
// See <http://creativecommons.org/publicdomain/zero/1.0/>.
// -------------------------------------------------------------------------

uint64_t pyro_splitmix64_next(pyro_splitmix64_state_t* state) {
    uint64_t result = (state->s += 0x9e3779b97f4a7c15);
    result = (result ^ (result >> 30)) * 0xbf58476d1ce4e5b9;
    result = (result ^ (result >> 27)) * 0x94d049bb133111eb;
    return result ^ (result >> 31);
}

// -------------------------------------------------------------------------
// END SplitMix64
// -------------------------------------------------------------------------


// Initializes the generator's state using SplitMix64, as recommended by the algorithm's
// authors (https://prng.di.unimi.it).
void pyro_xoshiro256ss_init(pyro_xoshiro256ss_state_t* state, uint64_t seed) {
    pyro_splitmix64_state_t sm64_state = { .s = seed };

    state->s[0] = pyro_splitmix64_next(&sm64_state);
    state->s[1] = pyro_splitmix64_next(&sm64_state);
    state->s[2] = pyro_splitmix64_next(&sm64_state);
    state->s[3] = pyro_splitmix64_next(&sm64_state);
}


// Generates a uniformly-distributed random integer on the half-open interval [0, n).
uint64_t pyro_xoshiro256ss_next_in_range(pyro_xoshiro256ss_state_t* state, uint64_t n) {
    if (n == 0 || n == 1) {
        return 0;
    }

    // Discarding values at or above [cutoff] avoids skew.
    uint64_t cutoff = (UINT64_MAX / n) * n;

    uint64_t rand_int;
    while ((rand_int = pyro_xoshiro256ss_next(state)) >= cutoff);

    return rand_int % n;
}
