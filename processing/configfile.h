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

#ifndef CONFIGFILE_H
#define CONFIGFILE_H 1

#include <stdbool.h>

#include "hashtable.h"

typedef CSC_HashTable CSC_Config;

// The main configuration
extern struct CSC_HashTable_ *csc_configTree;

// Load main configuration
void csc_configInit(const char *configFile);

// Load configuration files with the given name starting from the given directory
CSC_Config *csc_loadConfig(CORD dir, CORD file, bool parents);

/* Read a configuration directive with the given condition and the provided
 * (potentially NULL) variable definitions */
CORD csc_configRead(CSC_Config *config, CORD directive, CORD condition, CSC_HashTable *variables);

/* Reader for booleans */
bool csc_configBool(CSC_Config *config, CORD directive, CORD condition);

/* Reader for ints */
int csc_configInt(CSC_Config *config, CORD directive, CORD condition);

/* Reader for doubles */
double csc_configDouble(CSC_Config *config, CORD directive, CORD condition);

/* Simple read from default config */
CORD csc_config(CORD directive);

#endif
