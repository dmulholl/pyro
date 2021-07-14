// 64-bit Mersenne Twister PRNG (MT19937-64).
//
// The Mersenne Twister implementation in this module is adapted from the original implementation
// by the algorithm's inventors, Takuji Nishimura and Makoto Matsumoto, which was released under a
// 3-clause BSD license. See the accompanying readme.md file for the original source code comment
// and license text.
//
// Ref: http://www.math.sci.hiroshima-u.ac.jp/m-mat/MT/emt.html (2021-07-08)

#ifndef pyro_mt64_h
#define pyro_mt64_h

#include <inttypes.h>
#include <stdbool.h>

typedef struct MT64 MT64;

// Allocates a new generator.
MT64* pyro_new_mt64(void);

// Frees a generator.
void pyro_free_mt64(MT64* mt);

// Initializes a generator with a seed value.
void pyro_init_mt64(MT64* mt, uint64_t seed);

// Generates a random integer on the closed interval [0, 2^64 - 1].
uint64_t pyro_mt64_gen_u64(MT64* mt);

// Generates a random integer on the closed interval [0, 2^63 - 1].
int64_t pyro_mt64_gen_i64(MT64* mt);

// Generates a random double on the closed interval [0.0, 1.0] with 53 bits of precision.
double pyro_mt64_gen_f64a(MT64* mt);

// Generates a random double on the half-open interval [0.0, 1.0) with 53 bits of precision.
double pyro_mt64_gen_f64b(MT64* mt);

// Generates a random double on the open interval (0.0, 1.0) with 52 bits of precision.
double pyro_mt64_gen_f64c(MT64* mt);

// Generator test: verifies the 1st and 1000th output values for a known seed.
bool pyro_test_mt64(void);

#endif
