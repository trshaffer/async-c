#include <stdio.h>

#include "async.h"

ASYNC_DEF(int, foo, int, x) {
    return x + 1;
}

ASYNC_DEF(int, amain, int, argc) {
    ASYNC(f, foo, 100);
    int out = AWAIT(f);
    ASYNC_FREE(f);
    printf("%d\n", out);
    return argc;
}

ASYNC_MAIN(amain)
