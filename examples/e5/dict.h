#ifndef DICT_H
#define DICT_H

#include <stdio.h>

struct dict_entry_s {
    struct dict_entry_s *next;
    char *name;
    size_t len;
};
typedef struct dict_entry_s dict_entry_t;

typedef struct {
    dict_entry_t **table;
    size_t size;
} dict_t;

void dict_init(dict_t *dict, size_t size);

bool dict_exists(dict_t *dict, char *name, size_t len);

void dict_add(dict_t *dict, char *name, size_t len);

#endif
