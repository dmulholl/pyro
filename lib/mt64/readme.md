# 64-bit Mersenne Twister PRNG (MT19937-64)

A 64-bit Mersenne Twister pseudo-random number generator.

Ref: http://www.math.sci.hiroshima-u.ac.jp/m-mat/MT/emt.html (2021-07-08)

Example:

```c
#include "mt64.h"
#include <stdio.h>

int main(int argc, char** argv) {
    MT64 mt;
    mt64_init(&mt);

    uint64_t rand = mt64_gen_u64(&mt);
    printf("number: %zu\n", rand);

    return 0;
}
```
