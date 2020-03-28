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

#include "arg.h"

#include "nrepel.h"

#include "licenses.h"

#define FRAME_SIZE 960

void usage()
{
    fprintf(stderr, "Use: plip-noiserepellentdenoise [-i|--input <input file>] [-o|--output <output file>] [channels]\n\n");
}

int main(int argc, char **argv)
{
    size_t frameCt, frameSz;
    float *rawFrame;
    float *inFrame, *outFrame, latency;
    int i, oi, ci;
    int channels = 1;
    ssize_t rd, total;
    void **sts;
    char *inFile = NULL, *outFile = NULL;
    int inFd = 0, outFd = 1;

    ARG_VARS;

    fprintf(stderr, "The noise-repellent library used by this software is licensed under the following terms:\n\n%s\n---\n\n", fftw_license);
    fprintf(stderr, "The fftw library used by this software is licensed under the following terms:\n\n%s\n---\n\n", nrepel_license);

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
            );
        if (outFd < 0) {
            perror(outFile);
            return 1;
        }
    }

    // Allocate our frames
    frameCt = FRAME_SIZE * channels;
    frameSz = frameCt * sizeof(float);
    rawFrame = malloc(frameSz);
    if (!rawFrame) {
        perror("malloc");
        return 1;
    }
    inFrame = malloc(FRAME_SIZE * sizeof(float));
    if (!inFrame) {
        perror("malloc");
        return 1;
    }
    outFrame = malloc(FRAME_SIZE * sizeof(float));
    if (!outFrame) {
        perror("malloc");
        return 1;
    }

    // And our denoiser state
    sts = malloc(channels * sizeof(void *));
    if (!sts) {
        perror("malloc");
        return 1;
    }
    for (ci = 0; ci < channels; ci++) {
        sts[ci] = nrepel_instantiate(48000);
        nrepel_connect_port(sts[ci], NREPEL_LATENCY, &latency);
        nrepel_connect_port(sts[ci], NREPEL_INPUT, inFrame);
        nrepel_connect_port(sts[ci], NREPEL_OUTPUT, outFrame);
        float whitening = 25;
        nrepel_connect_port(sts[ci], NREPEL_WHITENING, &whitening);
    }

    while (1) {
        // Read in a frame
        total = 0;
        while (total < frameSz) {
            rd = read(inFd, ((char *) rawFrame) + total, frameSz - total);
            if (rd <= 0)
                break;
            total += rd;
        }
        if (total == 0)
            break;
        memset(((char *) rawFrame) + total, 0, frameSz - total);

        // Go channel by channel
        for (ci = 0; ci < channels; ci++) {
            // Extract one channel
            for (i = ci, oi = 0;
                 i < frameCt;
                 (i += channels), oi++) {
                inFrame[oi] = rawFrame[i];
            }

            // Process it
            nrepel_run(sts[ci], FRAME_SIZE);

            // Convert it back
            for (i = ci, oi = 0;
                 i < frameCt;
                 (i += channels), oi++) {
                rawFrame[i] = outFrame[oi];
            }
        }

        // Now output it again
        write(outFd, rawFrame, frameSz);
    }

    for (ci = 0; ci < channels; ci++)
        nrepel_cleanup(sts[ci]);

    free(sts);
    free(inFrame);
    free(outFrame);
    free(rawFrame);

    return 0;
}
