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


// Simple hash combiner. Returns h1 * 3 + h2. This is safer than simply using XOR to combine the
// hashes as XOR maps pairwise identical values to zero.
static inline uint64_t hash_combine(uint64_t h1, uint64_t h2) {
    return (h1 << 1) + h1 + h2;
}


// Generates an automatic seed value using locally available sources of entropy. This is intended
// to be fast and cross-platform rather than of especially high quality.
static uint64_t gen_auto_seed(MT64* mt) {
    // This is the default seed from the original algorithm.
    uint64_t seed = UINT64_C(5489);

    // The current unix timestamp in seconds.
    seed = hash_combine(seed, (uint64_t)time(NULL));

    // The number of clock ticks since the program was launched.
    seed = hash_combine(seed, (uint64_t)clock());

    // The next available heap address.
    void* malloc_addr = malloc(sizeof(int));
    free(malloc_addr);
    seed = hash_combine(seed, (uint64_t)malloc_addr);

    // The address of a local variable.
    seed = hash_combine(seed, (uint64_t)&seed);

    // The address of the generator instance itself.
    seed = hash_combine(seed, (uint64_t)mt);

    // The address of a local function.
    seed = hash_combine(seed, (uint64_t)&gen_auto_seed);

    // The address of a standard library function.
    seed = hash_combine(seed, (uint64_t)&time);

    return seed;
}


void mt64_init(MT64* mt) {
    mt->i = N + 1;
}


void mt64_seed_with_u64(MT64* mt, uint64_t seed) {
    mt->vec[0] = seed;
    for (mt->i = 1; mt->i < N; mt->i++) {
        mt->vec[mt->i] = F * (mt->vec[mt->i-1] ^ (mt->vec[mt->i-1] >> 62)) + mt->i;
    }
}


void mt64_seed_with_u64_array(MT64* mt, uint64_t array[], size_t array_length) {
    mt64_seed_with_u64(mt, UINT64_C(19650218));

    if (array_length == 0) {
        return;
    }

    size_t i = 1;
    size_t j = 0;

    uint64_t k = (N > array_length ? N : array_length);

    uint64_t n1 = UINT64_C(3935559000370003845);
    uint64_t n2 =  UINT64_C(2862933555777941757);

    for (; k; k--) {
        mt->vec[i] = (mt->vec[i] ^ ((mt->vec[i-1] ^ (mt->vec[i-1] >> 62)) * n1)) + array[j] + j;
        i++;
        j++;
        if (i >= N) {
            mt->vec[0] = mt->vec[N-1];
            i = 1;
        }
        if (j >= array_length) {
            j = 0;
        }
    }

    for (k = N - 1; k; k--) {
        mt->vec[i] = (mt->vec[i] ^ ((mt->vec[i-1] ^ (mt->vec[i-1] >> 62)) * n2)) - i;
        i++;
        if (i >= N) {
            mt->vec[0] = mt->vec[N-1];
            i = 1;
        }
    }

    mt->vec[0] = UINT64_C(1) << 63;
}


void mt64_seed_with_byte_array(MT64* mt, uint8_t array[], size_t array_length) {
    mt64_seed_with_u64_array(mt, (uint64_t*)array, array_length / 8);
}


uint64_t mt64_gen_u64(MT64* mt) {
    size_t j;
    uint64_t x;
    static uint64_t mag01[2] = {UINT64_C(0), A};

    // Generate N words at a time.
    if (mt->i >= N) {

        // If the generator hasn't already been initialized, initialize it with a default seed.
        if (mt->i == N + 1) {
            uint64_t seed = gen_auto_seed(mt);
            mt64_seed_with_u64(mt, seed);
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


int64_t mt64_gen_i64(MT64* mt) {
    return (int64_t)(mt64_gen_u64(mt) >> 1);
}


uint64_t mt64_gen_int(MT64* mt, uint64_t n) {
    if (n == 0 || n == 1) {
        return 0;
    }

    // Discarding values at or above [cutoff] avoids skew.
    uint64_t cutoff = (UINT64_MAX / n) * n;

    uint64_t rand_int;
    while ((rand_int = mt64_gen_u64(mt)) >= cutoff);

    return rand_int % n;
}


// The divisor is 2^53 - 1.
double mt64_gen_f64_cc(MT64* mt) {
    return (mt64_gen_u64(mt) >> 11) * (1.0/9007199254740991.0);
}


// The divisor is 2^53.
double mt64_gen_f64_co(MT64* mt) {
    return (mt64_gen_u64(mt) >> 11) * (1.0/9007199254740992.0);
}


// The divisor is 2^52.
double mt64_gen_f64_oo(MT64* mt) {
    return ((mt64_gen_u64(mt) >> 12) + 0.5) * (1.0/4503599627370496.0);
}


// Basic sanity check -- this verifies the 1st and 1000th output values for a known seed.
bool mt64_test(void) {
    MT64 mt;
    mt64_init(&mt);
    mt64_seed_with_u64(&mt, 5489);

    for (size_t i = 0; i < 1000; i++) {
        uint64_t num = mt64_gen_u64(&mt);
        if (i == 0 && num != UINT64_C(14514284786278117030)) {
            return false;
        }
        if (i == 999 && num != UINT64_C(10193180073869439881)) {
            return false;
        }
    }

    return true;
}
