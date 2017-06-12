#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <klee/klee.h>

#include "crc32.h"
#include "dict.h"

#define BUG() \
{ \
    char *p = NULL; \
    *p = 0; \
}

typedef struct {
    dict_t dict;
    char *input;
    bool eof;
} parser_t;

void parser_init(parser_t *parser, char *input) {
    dict_init(&parser->dict, 10);
    parser->input = input;
    parser->eof = false;
}

void parser_parse_token(parser_t *parser) {
    char *name = NULL;
    int len = 0;

    while (*parser->input != 0) {
        if (*parser->input != '\n') {
            break;
        }
        parser->input++;
    }

    if (*parser->input == 0) {
        parser->eof = true;
        return;
    }

    name = parser->input;
    while (*parser->input != 0) {
        if (*parser->input == '\n') {
            break;
        }
        len++;
        parser->input++;
    }

    if (len == 0) {
        parser->eof = true;
        return;
    }

    dict_add(&parser->dict, name, len);
}

#define PARSE_TOKEN(parser) \
    parser_parse_token(parser); \
    if (parser->eof) { \
        return; \
    }

void parser_parse_tokens(parser_t *parser) {
    for (unsigned int i = 0; i < 4; i++) {
        parser_parse_token(parser);
        if (parser->eof) {
            return;
        }
    }
}

void parser_parse(parser_t *parser) {
    parser_parse_tokens(parser);
    if (parser->eof) {
        return;
    }

    /* reachability test... */
    assert(false);
}

int main(int argc, char *argv[], char *envp[]) {
    parser_t parser;
    char buf[8];

    klee_make_symbolic(&buf, sizeof(buf), "buf");
    klee_assume(buf[sizeof(buf) - 1] == 0);

    parser_init(&parser, buf);
    parser_parse(&parser);

    return 0;
}
