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
#include "buffer.h"
#include "cscript.h"
#include "configfile.h"

BUFFER(charp, char *);

void usage()
{
    fprintf(stderr,
        "Use: plip-mix [-c|--config <config file>] [options] <output file> <video file> [audio files]\n"
        "Options:\n"
        "\t-o|--output-options <options> ; : ffmpeg output options. Must end\n"
        "\t                                  with a single semicolon\n"
        "\t                                  argument.\n"
        "\t-V|--video-filter <filters>: ffmpeg video filters\n"
        "\t-A|--audio-filter <track> <filters>: ffmpeg audio filters, per\n"
        "\t                                     audio track, 0-indexed\n"
        "\t-v|--verbose: Verbose mode\n\n");
}

int main(int argc, char **argv)
{
    ARG_VARS;
    CORD ffmpeg = "ffmpeg";

    csc_init(argv[0]);

    char *outFile = NULL;
    struct Buffer_charp outOptions;

    char *videoFile = NULL;
    const char *videoFilters = NULL;

    struct Buffer_charp audioFiles;
    struct Buffer_charp audioFilters;
    char *configFile = NULL;

    INIT_BUFFER(outOptions);
    INIT_BUFFER(audioFiles);
    INIT_BUFFER(audioFilters);

    ARG_NEXT();
    while (argType) {
        ARG(h, help) {
            usage();
            exit(0);
        } else ARG(v, verbose) {
            csc_verbose = true;
        } else ARGN(c, config) {
            ARG_GET();
            configFile = arg;
            if (!strcmp(configFile, "-"))
                configFile = NULL;

        } else ARGN(V, video-filter) {
            ARG_GET();
            videoFilters = arg;

        } else ARGN(A, audio-filter) {
            // Actually takes two args
            ARG_GET();
            int i = atoi(arg);
            ARG_NEXT();
            if (argType == ARG_NONE) {
                usage();
                exit(1);
            }
            char *filters = arg;

            while (audioFilters.bufused <= i)
                WRITE_ONE_BUFFER(audioFilters, "anull");
            audioFilters.buf[i] = filters;

        } else ARG(o, output-options) {
            // Takes arguments until ';'
            arg = argv[++argi];
            while (arg) {
                if (!strcmp(arg, ";"))
                    break;
                WRITE_ONE_BUFFER(outOptions, arg);
                arg = argv[++argi];
            }

        } else if (argType == ARG_VAL) {
            if (!outFile) {
                // Default to the output file
                outFile = arg;

            } else if (!videoFile) {
                // Then the video file
                videoFile = arg;
                if (!videoFilters)
                    videoFilters = "null";

            } else {
                // Else an audio file
                WRITE_ONE_BUFFER(audioFiles, arg);
                while (audioFilters.bufused < audioFiles.bufused)
                    WRITE_ONE_BUFFER(audioFilters, "anull");

            }

        } else {
            usage();
            exit(1);
        }
        ARG_NEXT();
    }

    if (!videoFile) {
        usage();
        exit(1);
    }

    // Get ffmpeg
    csc_configInit(configFile);
    ffmpeg = csc_config("programs.ffmpeg");

    // Form all the filters together
    CORD filters = csc_casprintf("[0:v]%s[vid]", videoFilters);
    for (int ai = 0; ai < audioFiles.bufused; ai++) {
        CORD afilter = csc_casprintf(";[%d:a]%s[aud%d]", ai+1, audioFilters.buf[ai], ai);
        filters = CORD_cat(filters, afilter);
    }

    // And the audio mixing filter
    if (audioFiles.bufused == 0) {
        filters = CORD_cat(filters, ";aevalsrc=0:s=48000:d=1[aud]");
    } else {
        filters = CORD_cat(filters, ";");
        for (int ai = 0; ai < audioFiles.bufused; ai++)
            filters = CORD_cat(filters, csc_casprintf("[aud%d]", ai));
        filters = CORD_cat(filters,
            csc_casprintf("amix=%d[aud]", audioFiles.bufused));
    }

    // Build the full arguments
    struct Buffer_charp args;
    INIT_BUFFER(args);
    WRITE_ONE_BUFFER(args, CORD_to_char_star(ffmpeg));
    WRITE_ONE_BUFFER(args, "-i");
    WRITE_ONE_BUFFER(args, videoFile);
    for (int ai = 0; ai < audioFiles.bufused; ai++) {
        WRITE_ONE_BUFFER(args, "-i");
        WRITE_ONE_BUFFER(args, audioFiles.buf[ai]);
    }
    WRITE_ONE_BUFFER(args, "-filter_complex");
    WRITE_ONE_BUFFER(args, CORD_to_char_star(filters));
    WRITE_ONE_BUFFER(args, "-map");
    WRITE_ONE_BUFFER(args, "[vid]");
    WRITE_ONE_BUFFER(args, "-map");
    WRITE_ONE_BUFFER(args, "[aud]");
    for (int oi = 0; oi < outOptions.bufused; oi++)
        WRITE_ONE_BUFFER(args, outOptions.buf[oi]);
    WRITE_ONE_BUFFER(args, outFile);
    WRITE_ONE_BUFFER(args, NULL);

    // Then run it
    return csc_run(0, NULL, args.buf);
}
