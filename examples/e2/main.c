#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <klee/klee.h>

#define BUG() \
{ \
    char *p = NULL; \
    *p = 0; \
}

#define SIZE 10

int target(char buf[SIZE], size_t size) {
    int s = 0;

    for (unsigned int i = 0; i < size; i++) {
        buf[i] = i % 2;
        s += buf[i];
    }

    return s;
}

int main(int argc, char *argv[]) {
    char buf[SIZE];

    target(buf, sizeof(buf));
    if (buf[0] == 1) {
        BUG();
    }
    if (buf[1] == 1) {
        BUG();
    }

    return 0;
}
