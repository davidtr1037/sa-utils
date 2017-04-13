#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "dict.h"
#include "crc32.h"

void dict_init(dict_t *dict, size_t size) {
    dict->table = calloc(size, sizeof(dict->table[0]));
    dict->size = size;

    for (unsigned int i = 0; i < size; i++) {
        dict->table[i] = NULL;
    }
}

bool dict_exists(dict_t *dict, char *name, size_t len) {
    /* compute hash */
    unsigned int h = crc32(0, name, len);
    h = h % dict->size; 

    dict_entry_t *current = dict->table[h];
    while (current) {
        if (strcmp(name, current->name) == 0) {
            /* found... */
            return true;
        }
        current = current->next;
    }

    return false;
}

void dict_add(dict_t *dict, char *name, size_t len) {
    /* compute hash */
    unsigned int h = crc32(0, name, len);
    h = h % dict->size; 

    if (dict_exists(dict, name, len)) {
        return;
    }

    /* allocate new entry */
    dict_entry_t *new_entry = calloc(1, sizeof(*new_entry));
    new_entry->name = strndup(name, len);
    new_entry->len = len;
    new_entry->next = NULL;

    /* insert new entry */
    dict_entry_t *head = dict->table[h];
    dict->table[h] = new_entry;
    new_entry->next = head;
}
