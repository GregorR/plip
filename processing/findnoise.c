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

#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef _WIN32
#include <io.h>
#endif

#include "arg.h"

#define FRAME_SIZE 48000

void usage()
{
    fprintf(stderr, "Use: plip-findnoise [-i|--input <input file>] [-o|--output <output file>] [channels]\n\n");
}

// Sample-based fabs
float sabs(float s)
{
    if (s < 0) {
        return -s;
    } else if (s == 0) {
        // To avoid counting digital silence, we imagine this to be 1
        return 1;
    } else {
        return s;
    }
}

// Allocate an array of floats
float *allocFloatArr(size_t sz, float val)
{
    size_t i;
    float *ret = malloc(sz*sizeof(float));
    if (ret == NULL) {
        perror("malloc");
        exit(1);
    }
    for (i = 0; i < sz; i++)
        ret[i] = val;
    return ret;
}

// Allocate an array of arrays of floats
float **allocFloatArr2(size_t sz1, size_t sz2, float val)
{
    size_t i;
    float **ret = malloc(sz1*sizeof(float *));
    if (ret == NULL) {
        perror("malloc");
        exit(1);
    }
    for (i = 0; i < sz1; i++)
        ret[i] = allocFloatArr(sz2, val);
    return ret;
}

// Free an array of arrays of floats
void freeFloatArr2(float **arr, size_t sz)
{
    size_t i;
    for (i = 0; i < sz; i++)
        free(arr[i]);
    free(arr);
}

int main(int argc, char **argv)
{
    float **curFrame;
    float **selFrame;
    float *curVolume;
    float *selVolume;
    int *curOffset;
    int *selOffset;
    int i, ci;
    int channels = 1;
    ssize_t rd, total;
    float s, sa;
    char *inFile = NULL, *outFile = NULL;
    int inFd = 0, outFd = 1;

    ARG_VARS;

#ifdef _WIN32
    setmode(0, O_BINARY);
    setmode(1, O_BINARY);
#endif

    ARG_NEXT();
    while (argType) {
        ARG(h, help) {
            usage();
            return 0;
        } else ARGN(i, input) {
            ARG_GET();
            inFile = arg;
        } else ARGN(o, output) {
            ARG_GET();
            outFile = arg;
        } else ARGN(c, channels) {
            ARG_GET();
            channels = atoi(arg);
        } else if (argType == ARG_VAL) {
            channels = atoi(arg);
        } else {
            usage();
            return 1;
        }
        ARG_NEXT();
    }

    // Set up I/O
    if (inFile) {
        inFd = open(inFile, O_RDONLY
#ifdef _WIN32
            |O_BINARY
#endif
            );
        if (inFd < 0) {
            perror(inFile);
            return 1;
        }
    }
    if (outFile) {
        outFd = open(outFile, O_WRONLY|O_CREAT
#ifdef _WIN32
            |O_BINARY
#endif
            , 0666);
        if (outFd < 0) {
            perror(outFile);
            return 1;
        }
    }

    // Allocate our frames
    curFrame = allocFloatArr2(channels, FRAME_SIZE, 0);
    selFrame = allocFloatArr2(channels, FRAME_SIZE, 0);
    curVolume = allocFloatArr(channels, 0);
    selVolume = allocFloatArr(channels, FRAME_SIZE);
    curOffset = calloc(channels, sizeof(int));
    if (curOffset == NULL) {
        perror("calloc");
        return 1;
    }
    selOffset = calloc(channels, sizeof(int));
    if (selOffset == NULL) {
        perror("calloc");
        return 1;
    }

    while (1) {
        for (ci = 0; ci < channels; ci++) {
            // Remove the current sample
            s = curFrame[ci][curOffset[ci]];
            sa = sabs(s);
            curVolume[ci] -= sa;

            // Read in a sample
            total = 0;
            while (total < sizeof(float)) {
                rd = read(inFd, ((char *) &s) + total, sizeof(float) - total);
                if (rd <= 0)
                    break;
                total += rd;
            }
            if (total < sizeof(float))
                break;
            curFrame[ci][curOffset[ci]] = s;
            sa = sabs(s);
            curVolume[ci] += sa;

            // Maybe select it
            if (curVolume[ci] < selVolume[ci]) {
                memcpy(selFrame[ci], curFrame[ci], FRAME_SIZE * sizeof(float));
                selOffset[ci] = curOffset[ci];
                selVolume[ci] = curVolume[ci];
            }

            // And advance
            curOffset[ci]++;
            if (curOffset[ci] >= FRAME_SIZE)
                curOffset[ci] = 0;
        }

        if (ci < channels)
            break;
    }

    // Write out whatever we selected
    for (i = 0; i < FRAME_SIZE; i++) {
        for (ci = 0; ci < channels; ci++) {
            s = selFrame[ci][(i+selOffset[ci])%FRAME_SIZE];
            write(outFd, &s, sizeof(float));
        }
    }

    // Clean up
    freeFloatArr2(curFrame, channels);
    freeFloatArr2(selFrame, channels);
    free(curOffset);
    free(selOffset);
    free(curVolume);
    free(selVolume);

    if (inFd != 0)
        close(inFd);
    if (outFd != 1)
        close(outFd);

    return 0;
}
