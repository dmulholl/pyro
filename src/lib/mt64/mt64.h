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
#include <stddef.h>

typedef struct MT64 MT64;

// Allocates a new generator.
MT64* pyro_mt64_new(void);

// Frees a generator.
void pyro_mt64_free(MT64* mt);

// (Re)initializes the generator's state vector using a seed value.
void pyro_mt64_seed_with_u64(MT64* mt, uint64_t seed);

// (Re)intializes the generator's state vector using an array of seed values.
// Uses up to the first 312 values from the array.
void pyro_mt64_seed_with_u64_array(MT64* mt, uint64_t array[], size_t array_length);

// (Re)intializes the generator's state vector using an array of seed values.
// Uses up to the first [312 * 8] values from the array.
// The array should contain at least 8 bytes.
void pyro_mt64_seed_with_byte_array(MT64* mt, uint8_t array[], size_t array_length);

// Generates a random integer on the closed interval [0, 2^64 - 1].
uint64_t pyro_mt64_gen_u64(MT64* mt);

// Generates a random integer on the closed interval [0, 2^63 - 1].
int64_t pyro_mt64_gen_i64(MT64* mt);

// Generates a uniformly-distributed random integer on the half-open interval [0, n).
uint64_t pyro_mt64_gen_int(MT64* mt, uint64_t n);

// Generates a random double on the closed interval [0.0, 1.0] with 53 bits of precision.
double pyro_mt64_gen_f64a(MT64* mt);

// Generates a random double on the half-open interval [0.0, 1.0) with 53 bits of precision.
double pyro_mt64_gen_f64b(MT64* mt);

// Generates a random double on the open interval (0.0, 1.0) with 52 bits of precision.
double pyro_mt64_gen_f64c(MT64* mt);

// Generator test: verifies the 1st and 1000th output values for a known seed.
bool pyro_mt64_test(void);

#endif
