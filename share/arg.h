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

#ifndef ARG_H
#define ARG_H

#define ARG_VARS \
    char *arg = NULL; \
    int argi = 0, argType = 0

#define ARG_NONE  0
#define ARG_SHORT 1
#define ARG_LONG  2
#define ARG_VAL   3

#define ARG_NEXT() do { \
    if (argType == ARG_SHORT) \
        if (*++arg) break; \
    \
    arg = argv[++argi]; \
    if (!arg) { \
        argType = ARG_NONE; \
    } else if (!strncmp(arg, "--", 2)) {\
        argType = ARG_LONG; \
    } else if (arg[0] == '-') { \
        argType = ARG_SHORT; \
        arg++; \
    } else { \
        argType = ARG_VAL; \
    } \
} while (0)

#define ARGSC(s)        (argType == ARG_SHORT && *arg == #s[0])
#define ARGSNC(s)       (ARGSC(s) && (arg[1] || argv[argi+1]))
#define ARGLC(l)        (argType == ARG_LONG && !strcmp(arg, "--" #l))
#define ARGLNC(l)       (argType == ARG_LONG && \
                         (!strcmp(arg, "--" #l) || \
                          !strncmp(arg, "--" #l "=", sizeof("--" #l "=")-1)))

#define ARGS(s)         if ARGSC(s)
#define ARGSN(s)        if ARGSNC(s)
#define ARGL(l)         if ARGLC(l)
#define ARGLN(l)        if ARGLNC(l)
#define ARG(s, l)       if (ARGSC(s) || ARGLC(l))
#define ARGN(s, l)      if (ARGSNC(s) || ARGLNC(l))

#define ARG_GET() do { \
    if (argType == ARG_SHORT && arg[1]) { \
        arg++; \
    } else if (argType == ARG_LONG && (arg = strchr(arg, '='))) { \
        arg++; \
    } else { \
        arg = argv[++argi]; \
    } \
    argType = ARG_VAL; \
} while (0)

#define ARGSV(s, v)     ARGS(s) { (v) = 1; } else
#define ARGSNV(s, v)    ARGSN(s) { ARG_GET(); (v) = arg; } else
#define ARGLV(l, v)     ARGL(l) { (v) = 1; } else
#define ARGLNV(l, v)    ARGLN(l) { ARG_GET(); (v) = arg; } else
#define ARGV(s, l, v)   ARG(s, l) { (v) = 1; } else
#define ARGNV(s, l, v)  ARGN(s, l) { ARG_GET(); (v) = arg; } else

#endif
