# 64-bit Mersenne Twister PRNG (MT19937-64)

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
