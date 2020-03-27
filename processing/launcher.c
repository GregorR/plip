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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "arg.h"
#include "cscript.h"

void usage()
{
    fprintf(stderr,
        "Use: plip [-c|--config <config file>] [-v] [-i] <input file>\n"
        "Options:\n"
        "\t-v|--verbose: Verbose mode\n\n");
}

void slowexit(int val)
{
    char c;
    read(0, &c, 1);
    exit(val);
}

int main(int argc, char **argv)
{
    ARG_VARS;

    csc_init(argv[0]);

    ARG_NEXT();
    char *inputFile = NULL, *configFile = "-";
    while (argType) {
        ARG(h, help) {
            usage();
            exit(0);
        } else ARG(v, verbose) {
            csc_verbose = true;
        } else ARGN(i, input-file) {
            ARG_GET();
            inputFile = arg;
        } else ARGN(c, config) {
            ARG_GET();
            configFile = arg;
        } else if (argType == ARG_VAL) {
            if (!inputFile)
                inputFile = arg;
            else {
                usage();
                exit(1);
            }
        } else {
            usage();
            exit(1);
        }
        ARG_NEXT();
    }

    if (!inputFile) {
        usage();
        exit(1);
    }

    csc_runl(0, NULL, "plip-demux", "-c", configFile, inputFile, NULL);
    csc_runl(0, NULL, "plip-aproc", "-c", configFile, NULL);
    csc_runl(0, NULL, "plip-clip", "-C", configFile, inputFile, NULL);
    printf("\nComplete.\n");
    slowexit(0);
    return 0;
}
