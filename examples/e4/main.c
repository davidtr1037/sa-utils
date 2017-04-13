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

typedef struct {
    char *input;
    bool eof;
} parser_t;

void parser_init(parser_t *parser, char *input) {
    parser->input = input;
    parser->eof = false;
}

void parser_parse_name(parser_t *parser) {
    /* skip new lines... */
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
}

int main(int argc, char *argv[]) {
    parser_t parser;
    char buf[10];

    klee_make_symbolic(&buf, sizeof(buf), "buf");
    klee_assume(buf[sizeof(buf) - 1] == 0);

    parser_init(&parser, buf);
    parser_parse_name(&parser);
    if (parser.eof) {
        BUG();
    }

    return 0;
}
