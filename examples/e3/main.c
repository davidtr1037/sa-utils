#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <klee/klee.h>

#include "crc32.h"

#define BUG() \
{ \
    char *p = NULL; \
    *p = 0; \
}

unsigned int parse_lines(char *buf, size_t size) {
    unsigned int count = 0;

    for (unsigned int i = 0; i < size; i++) {
        if (buf[i] == '\n') {
            count++;
        }
    }

    return count;
}

void target(char *buf, size_t size, int *lines) {
    *lines = parse_lines(buf, size);

    unsigned int h = crc32(0, buf, size);
    if (h == 17) {
        printf("OK...\n");
    } else {
        printf("ERROR...\n");
    }
}

int main(int argc, char *argv[]) {
    char buf[8];
    int lines = 0;

    klee_make_symbolic(&buf, sizeof(buf), "buf");

    target(buf, sizeof(buf), &lines);
    //if (lines == 1) {
    //    BUG();
    //}

    return 0;
}
