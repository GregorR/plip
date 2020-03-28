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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "arg.h"

#include "speex/speex_preprocess.h"

#include "licenses.h"

#define FRAME_SIZE 960

void usage()
{
    fprintf(stderr, "Use: plip-speexdenoise [-i|--input <input file>] [-o|--output <output file>] [channels]\n\n");
}

int main(int argc, char **argv)
{
    size_t frameCt, frameSz;
    short *rawFrame;
    short *frame;
    int i, oi, ci;
    int channels = 1;
    ssize_t rd, total;
    SpeexPreprocessState **sts;
    char *inFile = NULL, *outFile = NULL;
    int inFd = 0, outFd = 1;

    ARG_VARS;

    fprintf(stderr, "The speexdsp-denoise library used by this software is licensed under the following terms:\n\n%s\n---\n\n", speexdsp_license);

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
    frameSz = frameCt * sizeof(short);
    rawFrame = malloc(frameSz);
    if (!rawFrame) {
        perror("malloc");
        return 1;
    }
    frame = malloc(FRAME_SIZE * sizeof(short));
    if (!frame) {
        perror("malloc");
        return 1;
    }

    // And our denoiser state
    sts = malloc(channels * sizeof(SpeexPreprocessState *));
    if (!sts) {
        perror("malloc");
        return 1;
    }
    for (ci = 0; ci < channels; ci++) {
        sts[ci] = speex_preprocess_state_init(FRAME_SIZE, 48000);
        i=1;
        speex_preprocess_ctl(sts[ci], SPEEX_PREPROCESS_SET_DENOISE, &i);
        i=0;
        speex_preprocess_ctl(sts[ci], SPEEX_PREPROCESS_SET_AGC, &i);
        i=0;
        speex_preprocess_ctl(sts[ci], SPEEX_PREPROCESS_SET_DEREVERB, &i);
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
                frame[oi] = rawFrame[i];
            }

            // Process it
            speex_preprocess_run(sts[ci], frame);

            // Convert it back
            for (i = ci, oi = 0;
                 i < frameCt;
                 (i += channels), oi++) {
                rawFrame[i] = frame[oi];
            }
        }

        // Now output it again
        write(outFd, rawFrame, frameSz);
    }

    for (ci = 0; ci < channels; ci++)
        speex_preprocess_state_destroy(sts[ci]);

    free(sts);
    free(frame);
    free(rawFrame);

    return 0;
}
