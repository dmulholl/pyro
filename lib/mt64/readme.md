# 64-bit Mersenne Twister PRNG (MT19937-64)

[1]: http://www.math.sci.hiroshima-u.ac.jp/m-mat/MT/emt64.html


A 64-bit Mersenne Twister pseudo-random number generator.

Example:

```c
int main(int argc, char** argv) {
    MT64 mt;
    mt64_init(&mt);

    for (int i = 0; i < 10; i++) {
        uint64_t rand = mt64_gen_u64(&mt);
        printf("%llu\n", rand);
    }

    return 0;
}
```


## Usage

Initialize a new generator:

```c
void mt64_init(MT64* mt);
```

Seed/re-seed the generator (optional):

```c
void mt64_seed_with_u64(MT64* mt, uint64_t seed);
```

If you don't seed the generator, it will be automatically seeded on first use with a random seed.

Generate a random number:

```c
// Generates a uniformly-distributed random integer on the closed interval [0, 2^64 - 1].
uint64_t mt64_gen_u64(MT64* mt);

// Generates a uniformly-distributed random integer on the closed interval [0, 2^63 - 1].
int64_t mt64_gen_i64(MT64* mt);

// Generates a uniformly-distributed random integer on the half-open interval [0, n).
uint64_t mt64_gen_int(MT64* mt, uint64_t n);

// Generates a uniformly-distributed random double on the closed interval [0.0, 1.0]
// with 53 bits of precision.
double mt64_gen_f64_cc(MT64* mt);

// Generates a uniformly-distributed random double on the half-open interval [0.0, 1.0)
// with 53 bits of precision.
double mt64_gen_f64_co(MT64* mt);

// Generates a uniformly-distributed random double on the open interval (0.0, 1.0)
// with 52 bits of precision.
double mt64_gen_f64_oo(MT64* mt);
```


## License

All code is released under the 3-clause BSD license.

This library is based on the [original implementation][1] by the algorithm's inventors, Takuji
Nishimura and Makoto Matsumoto, which was released under the 3-clause BSD license.

