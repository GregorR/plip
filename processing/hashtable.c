/*
 * Copyright (c) 2020 Gregor Richards
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>

#include "gc.h"

#include "hashtable.h"

#define HT_DEFAULT_SIZE 4

// We use the sdbm hash function
static unsigned long sdbm(unsigned char *str)
{
    unsigned long hash = 0;
    long c;

    while ((c = *str++))
        hash = c + (hash << 6) + (hash << 16) - hash;

    return hash;
}

// Allocate a hash table
CSC_HashTable *csc_newHashTable(void)
{
    CSC_HashTable *ret = GC_NEW(CSC_HashTable);
    ret->table = GC_MALLOC(HT_DEFAULT_SIZE * sizeof(CSC_HashEntry));
    ret->sz = HT_DEFAULT_SIZE;
    ret->used = 0;
    ret->mask = HT_DEFAULT_SIZE-1;
    return ret;
}

// Expand a hash table
static void expand(CSC_HashTable *ht)
{
    CSC_HashEntry *oldTable = ht->table;
    size_t oldSz = ht->sz;

    // Reallocate to a larger size
    ht->sz *= 2;
    ht->used = 0;
    ht->mask = (ht->mask<<2)+1;
    ht->table = GC_MALLOC(ht->sz * sizeof(CSC_HashEntry));

    // Re-add all the entries
    for (size_t ti = 0; ti < oldSz; ti++) {
        CSC_HashEntry *he = &oldTable[ti];
        while (he) {
            if (he->value)
                csc_htAddHash(ht, he->key, he->hash, he->value);
            he = he->next;
        }
    }
}

// Add or replace an entry to a hash table. Add NULL to remove an entry.
void csc_htAdd(CSC_HashTable *ht, CORD key, void *value)
{
    char *ckey = CORD_to_char_star(key);
    csc_htAddHash(ht, ckey, sdbm((unsigned char *) ckey), value);
}

// Add an entry when we already know the hash
void csc_htAddHash(CSC_HashTable *ht, char *key, unsigned long hash, void *value)
{
    // Expand if we should
    if (ht->used >= ht->sz)
        expand(ht);

    // Choose a slot
    size_t slot = hash & ht->mask;
    CSC_HashEntry *he = &ht->table[slot];
    while (he) {
        if (he->value) {
            // Should we replace this?
            if (he->hash == hash && !strcmp(he->key, key)) {
                // Yes
                he->value = value;
                return;
            }

        } else {
            // Should we add it here?
            if (!he->next) {
                // Yes
                he->key = key;
                he->hash = hash;
                he->value = value;
                return;
            }

        }

        // Should we create a new slot for it?
        if (!he->next)
            he->next = GC_NEW(CSC_HashEntry);

        he = he->next;
    }

    // If we got here, something went terribly wrong
    fprintf(stderr, "Error adding entry to hash table!\n");
    exit(1);
}

// Get an entry or NULL if it's not present
void *csc_htGet(CSC_HashTable *ht, CORD key)
{
    char *ckey = CORD_to_char_star(key);
    return csc_htGetHash(ht, ckey, sdbm((unsigned char *) ckey));
}

// Get an entry when we already know the hash
void *csc_htGetHash(CSC_HashTable *ht, char *key, unsigned long hash)
{
    // Choose a slot
    size_t slot = hash & ht->mask;
    CSC_HashEntry *he = &ht->table[slot];
    while (he) {
        // Check if it matches
        if (he->hash == hash && !strcmp(he->key, key))
            return he->value;
        he = he->next;
    }

    return NULL;
}
