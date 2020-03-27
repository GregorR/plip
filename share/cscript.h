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

#ifndef CSCRIPT_H
#define CSCRIPT_H 1

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <pcre.h>

#include "gc.h"
#include "cord.h"

/* Constants used to specify stdio capture */
#define CSC_STDIN       (1<<0)
#define CSC_STDOUT      (1<<1)
#define CSC_STDERR      (1<<2)
#define CSC_STDOUTERR   (CSC_STDOUT|CSC_STDERR)

/* Directory separator */
#ifdef _WIN32
#define CSC_DIRSEP "\\"
#else
#define CSC_DIRSEP "/"
#endif

/* General "quit now" */
#define CRASH(fault) do { \
    perror(fault); \
    exit(1); \
} while(0)

/* Should we be verbose? */
extern bool csc_verbose;

/* cscript initialization tasks */
void csc_init(const char *arg0);

#ifdef _WIN32
/* Windows is trash; this quotes its arguments nicely. */
CORD csc_windowsArgs(char *const argv[]);
#endif

/* Like CORD_sprintf, but just return the cord directly */
CORD csc_casprintf(CORD format, ...);
CORD csc_cvasprintf(CORD format, va_list args);

/* Similar, but drop to a regular string */
char *csc_asprintf(CORD format, ...);
char *csc_vasprintf(CORD format, va_list args);

/* Does this file exist? */
bool csc_fileExists(CORD filename);

/* Portable globbing, just returns a NULL-terminated CORD array */
CORD *csc_glob(CORD pattern);

/* Absolute-ize a path */
CORD csc_absolute(CORD path);

/* Get the dirname (INCLUDING terminating /) from a path */
CORD csc_dirname(CORD path);

/* Portable link */
void csc_ln(CORD from, CORD to);

/* Read an entire file */
CORD csc_readFile(CORD filename);

/* Write an entire file */
bool csc_writeFile(CORD filename, CORD content);

/* Run a program and capture its output into a cord. Returns the exit code of
 * the program, or -1 for errors. Set STDIN in fds to NOT redirect input from
 * /dev/null. Set STDOUT or STDERR in fds to capture them. */
int csc_run(int fds, CORD *into, char *const argv[]);

/* Wrapper for csc_run to take the arguments directly */
int csc_runl(int fds, CORD *into, const char *arg, ...);

/* Run a program with its input and output piped. Provide an fd greater than -1
 * as stdinFd to pipe input, an output fd is returned or -1 for error. If
 * stdinFd is provided, it is closed. */
int csc_runp(int stdinFd, char *const argv[]);

/* runp with arguments directly */
int csc_runpl(int stdinFd, const char *arg, ...);

/* Wait for a pipeline by way of waiting for a pipe */
bool csc_wait(int fd);

/* Match a string against a regex. Returns all matches as an array. */
CORD *csc_match(CORD pattern, CORD input);

/* Split input by line */
CORD *csc_lines(CORD input);

/* Select lines matching the given PCRE regex */
CORD *csc_grep(CORD pattern, CORD input);

#endif
