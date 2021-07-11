// 64-bit Mersenne Twister PRNG (MT19937-64).
//
// The code in this module is adapted from the original implementation by the algorithm's inventors,
// Takuji Nishimura and Makoto Matsumoto, which was released under a 3-clause BSD license. See the
// accompanying readme.md file for the original source code comment and license text.
//
// Ref: http://www.math.sci.hiroshima-u.ac.jp/m-mat/MT/emt.html (2021-07-08)

#include "mt64.h"

#include <time.h>
#include <stdlib.h>


// Degree of recurrence.
#define N 312

// Middle word.
#define M 156

// Twist matrix coefficient.
#define A UINT64_C(0xB5026F5AA96619E9)

// Bit mask for the most significant 33 bits.
#define UM UINT64_C(0xFFFFFFFF80000000)

// Bit mask for the least significant 31 bits.
#define LM UINT64_C(0x7FFFFFFF)

// Initialization constant.
#define F UINT64_C(6364136223846793005)


// Stores the state vector and the current index into that vector.
struct MT64 {
    uint64_t vec[N];
    size_t i;
};


// Allocates a new generator.
MT64* pyro_new_mt64(void) {
    MT64* mt = malloc(sizeof(MT64));
    if (!mt) {
        return NULL;
    }
    mt->i = N + 1;
    return mt;
}


// Frees a generator.
void pyro_free_mt64(MT64* mt) {
    free(mt);
}


// Initializes the generator's state vector with a seed value.
void pyro_init_mt64(MT64* mt, uint64_t seed) {
    mt->vec[0] = seed;
    for (mt->i = 1; mt->i < N; mt->i++) {
        mt->vec[mt->i] = F * (mt->vec[mt->i-1] ^ (mt->vec[mt->i-1] >> 62)) + mt->i;
    }
}


// Generates a random integer on the closed interval [0, 2^64 - 1].
uint64_t pyro_mt64_gen_u64(MT64* mt) {
    size_t j;
    uint64_t x;
    static uint64_t mag01[2] = {UINT64_C(0), A};

    // Generate N words at a time.
    if (mt->i >= N) {

        // If the generator hasn't already been initialized, initialize it with a default seed.
        if (mt->i == N + 1) {
            pyro_init_mt64(mt, time(NULL));
        }

        for (j = 0; j < N - M; j++) {
            x = (mt->vec[j] & UM) | (mt->vec[j+1] & LM);
            mt->vec[j] = mt->vec[j+M] ^ (x>>1) ^ mag01[(int)(x & UINT64_C(1))];
        }

        for (; j < N - 1; j++) {
            x = (mt->vec[j] & UM) | (mt->vec[j+1] & LM);
            mt->vec[j] = mt->vec[j+(M-N)] ^ (x>>1) ^ mag01[(int)(x & UINT64_C(1))];
        }

        x = (mt->vec[N-1] & UM) | (mt->vec[0] & LM);
        mt->vec[N-1] = mt->vec[M-1] ^ (x>>1) ^ mag01[(int)(x & UINT64_C(1))];

        mt->i = 0;
    }

    x = mt->vec[mt->i++];

    x ^= (x >> 29) & UINT64_C(0x5555555555555555);
    x ^= (x << 17) & UINT64_C(0x71D67FFFEDA60000);
    x ^= (x << 37) & UINT64_C(0xFFF7EEE000000000);
    x ^= (x >> 43);

    return x;
}


// Generates a random integer on the closed interval [0, 2^63 - 1].
int64_t pyro_mt64_gen_i64(MT64* mt) {
    return (int64_t)(pyro_mt64_gen_u64(mt) >> 1);
}


// Generates a random double on the closed interval [0, 1] with 53 bits of precision.
// (The divisor is 2^53 - 1.)
double pyro_mt64_gen_f64a(MT64* mt) {
    return (pyro_mt64_gen_u64(mt) >> 11) * (1.0/9007199254740991.0);
}


// Generates a random double on the half-open interval [0, 1) with 53 bits of precision.
// (The divisor is 2^53.)
double pyro_mt64_gen_f64b(MT64* mt) {
    return (pyro_mt64_gen_u64(mt) >> 11) * (1.0/9007199254740992.0);
}


// Generates a random double on the open interval (0, 1) with 52 bits of precision.
// (The divisor is 2^52.)
double pyro_mt64_gen_f64c(MT64* mt) {
    return ((pyro_mt64_gen_u64(mt) >> 12) + 0.5) * (1.0/4503599627370496.0);
}


// Basic sanity check -- this verifies the 1st and 1000th output values for a known seed.
bool pyro_test_mt64(void) {
    MT64* mt = pyro_new_mt64();
    pyro_init_mt64(mt, 5489);

    for (size_t i = 0; i < 1000; i++) {
        uint64_t num = pyro_mt64_gen_u64(mt);
        if (i == 0 && num != UINT64_C(14514284786278117030)) {
            pyro_free_mt64(mt);
            return false;
        }
        if (i == 999 && num != UINT64_C(10193180073869439881)) {
            pyro_free_mt64(mt);
            return false;
        }
    }

    pyro_free_mt64(mt);
    return true;
}
