#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include <klee/klee.h>

#include "md5.h"

#define BUG() \
{ \
    char *p = NULL; \
    *p = 0; \
}

void hash(void *data, size_t size, unsigned char digest[16]) {
    md5_state_t state;

    /* compute hash... */
    md5_init(&state);
    md5_append(&state, data, size);
    md5_finish(&state, digest);
}

size_t parse_token(char *buf, size_t size) {
    size_t len = 0;

    for (unsigned int i = 0; i < size; i++) {
        char c = buf[i];
        if (c == '\0' || c == '\n') {
            break;
        }
        len++;
    }

    return len; 
}

void check_token(char *buf, size_t token_size) {
    unsigned char digest[16];
    unsigned char correct_digest[16];

    /* get token hash */
    hash(buf, token_size, digest);

    /* expected hash */
    for (unsigned int i = 0; i < sizeof(correct_digest); i++) {
        correct_digest[i] = i;
    }

    /* compare... */
    if (memcmp(digest, correct_digest, 16) == 0) {
        printf("OK\n");
    }
}

int main(int argc, char *argv[]) {
    char buf[8];
    size_t token_size;

    klee_make_symbolic(&buf, sizeof(buf), "buf");

    token_size = parse_token(buf, sizeof(buf));
    check_token(buf, token_size);

    return 0;
}
