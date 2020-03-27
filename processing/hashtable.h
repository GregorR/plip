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

#ifndef HASHTABLE_H
#define HASHTABLE_H 1

#include "cord.h"

// An entry in a hash table
typedef struct CSC_HashEntry_ {
    struct CSC_HashEntry_ *next;
    char *key;
    unsigned long hash;
    void *value;
} CSC_HashEntry;

// The hash table itself
typedef struct CSC_HashTable_ {
    struct CSC_HashEntry_ *table;
    size_t sz, used;
    unsigned long mask;
} CSC_HashTable;

// Allocate a hash table
CSC_HashTable *csc_newHashTable(void);

// Add or replace an entry to a hash table. Add NULL to remove an entry.
void csc_htAdd(CSC_HashTable *ht, CORD key, void *value);

// Add an entry when we already know the hash
void csc_htAddHash(CSC_HashTable *ht, char *key, unsigned long hash, void *value);

// Get an entry or NULL if it's not present
void *csc_htGet(CSC_HashTable *ht, CORD key);

// Get an entry when we already know the hash
void *csc_htGetHash(CSC_HashTable *ht, char *key, unsigned long hash);

#endif
