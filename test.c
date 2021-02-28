#include <stdio.h>

#include "async.h"

ASYNC_DEF(int, foo, int, x) {
    printf("foo  %d\n", x);
    return x + 11;
}

//        int  amain (int  argc) {
ASYNC_DEF(int, amain, int, argc) {
    int rc = 0;

//  async f =foo( 100);
    ASYNC(f, foo, 100);
    ASYNC(g, foo, 200);
    ASYNC(h, foo, 300);

    printf("%p, %p, %p)\n", f, g, h);

    ASYNC_SPAWN(h);
    ASYNC_SPAWN(g);

    rc = 2;
    int out = AWAIT(f);
    ASYNC_FREE(f);

    printf("main %d\n", out);
    return rc;
}

ASYNC_MAIN(amain)
