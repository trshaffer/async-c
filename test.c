#include <stdio.h>

#include "async.h"

ASYNC_DEF(int, foo, int, x) {
    return x + 1;
}

int main() {
    ASYNC(f, foo, 100);
    int out = AWAIT(f);
    printf("%d\n", out);
    return 0;
}
