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

#define GC_THREADS 1
#define _POSIX_SOURCE 1

#include <pthread.h>
#include <string.h>

#include "arg.h"
#include "cscript.h"
#include "configfile.h"

static const CORD quote = "^[^\"]*\"(.*)\"$";
static const CORD equals = "^[^=]*=(.*)$";
static CORD ffmpeg = "ffmpeg";
static CORD ffprobe = "ffprobe";
static CORD iformat = "flac";
static CORD icodec = "flac";

// Get the codec type of a file
CORD codecType(CORD streams, int idx)
{
    CORD *codec_type = csc_grep(csc_casprintf("^streams\\.stream\\.%d\\.codec_type", idx), streams);
    if (!codec_type || !codec_type[0]) return CORD_EMPTY;
    codec_type = csc_match(quote, codec_type[0]);
    if (!codec_type || !codec_type[1]) return CORD_EMPTY;
    return codec_type[1];
}

CORD title(CORD streams, int idx)
{
    CORD *title = csc_grep(csc_casprintf("^streams\\.stream\\.%d\\.tags\\.title", idx), streams);
    if (!title || !title[0]) return CORD_EMPTY;
    title = csc_match(quote, title[0]);
    if (!title || !title[1]) return CORD_EMPTY;
    return title[1];
}

// Fix an audio track's title
void fixTitle(int *others, CORD *title)
{
    if (!*title) {
        (*others)++;
        CORD_sprintf(title, "audio%d", *others);
    }
}

// Extract an audio track
void audio(const char *inputFile, int trackno, CORD title)
{
    CORD rawFlac, outName, procName;
    CORD_sprintf(&rawFlac, "%r-raw.flac", title); // Will need to use a different iformat than flac for this to be useful
    CORD_sprintf(&outName, "%r-raw.%r", title, iformat);
    CORD_sprintf(&procName, "%r-proc.%r", title, iformat);

    if (csc_fileExists(outName) || csc_fileExists(procName)) {
        // Already exists!
        CORD_fprintf(stderr, "^PLIP: %r already demuxed and/or processed.\n", title);
        return;
    }

    if (csc_fileExists(rawFlac)) {
        CORD_fprintf(stderr, "^PLIP: Extracting %r from %r.\n", outName, rawFlac);
        // already provided, just use the flac file
        csc_runl(0, NULL,
            ffmpeg, "-nostdin", "-i", CORD_to_char_star(rawFlac), "-c:a",
            icodec, "-ar", "48000", "-ac", "2",
            CORD_to_char_star(outName), NULL);

    } else {
        CORD_fprintf(stderr, "^PLIP: Extracting %r from %r.\n", outName, inputFile);
        // extract it
        csc_runl(0, NULL,
            ffmpeg, "-nostdin", "-copyts", "-i", inputFile,
            "-map", csc_asprintf("0:%d", trackno), "-af",
            csc_config("filters.resample"), "-c:a", icodec,
            "-ar", "48000", "-ac", "2", CORD_to_char_star(outName), NULL);

    }
}

// audio in a thread
struct AudioThread {
    const char *inputFile;
    int trackNo;
    CORD title;
};
void *audioThread(void *vat)
{
    struct AudioThread *at = vat;
    audio(at->inputFile, at->trackNo, at->title);
    return NULL;
}

void usage()
{
    fprintf(stderr,
        "Use: plip-demux [-v|--verbose] [-n|--dry-run] [-c|--config <config file>] <file>\n\n");
}

int main(int argc, char **argv)
{
    ARG_VARS;

    csc_init(argv[0]);

    const char *configFile = NULL;
    const char *inputFile = NULL;
    int dryRun = 0;
    ARG_NEXT();
    while (argType) {
        ARG(h, help) {
            usage();
            exit(0);
        } else ARGN(c, config) {
            ARG_GET();
            configFile = arg;
            if (!strcmp(arg, "-"))
                configFile = NULL;
        } else ARG(v, verbose) {
            csc_verbose = true;
        } else ARG(n, dry-run) {
            dryRun = 1;
        } else if (argType == ARG_VAL && !inputFile) {
            inputFile = arg;
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

    csc_configInit(configFile);
    ffmpeg = csc_config("programs.ffmpeg");
    ffprobe = csc_config("programs.ffprobe");
    iformat = csc_config("formats.aiformat");
    icodec = csc_config("formats.aicodec");

    // ffprobe output for format info
    CORD format = CORD_EMPTY;

    // ffprobe output for stream info
    CORD streams = CORD_EMPTY;

    // get the format info
    csc_runl(CSC_STDOUT, &format,
        ffprobe, "-print_format", "flat", "-show_format",
        inputFile, NULL);

    // and the stream info
    csc_runl(CSC_STDOUT, &streams,
        ffprobe, "-print_format", "flat", "-show_streams",
        inputFile, NULL);

    // get the number of streams
    int nbStreams = 0;
    CORD *streamInfo = csc_grep("^format\\.nb_streams", format);
    if (streamInfo && streamInfo[0]) {
        streamInfo = csc_match(equals, streamInfo[0]);
        if (streamInfo && streamInfo[1])
            nbStreams = atoi(CORD_to_char_star(streamInfo[1]));
    }
    fprintf(stderr, "^PLIP: %d streams\n", nbStreams);

    // Look for video tracks
    for (int si = 0; si < nbStreams; si++) {
        CORD stype = codecType(streams, si);
        CORD stitle = title(streams, si);

        if (!CORD_cmp(stype, "video")) {
            if (!CORD_cmp(stitle, NULL)) {
                /* We expect untitled tracks, and take this to mean "the
                 * important video" */
                stitle = "video";
            }

            // Ignore it if we must
            if (!csc_configBool(csc_configTree, "tracks.include", stitle)) {
                CORD_fprintf(stderr, "^PLIP: Video track %r (%d) excluded\n", stitle, si);
                continue;
            }

            // Remember it in a .track file
            if (!dryRun) {
                csc_writeFile(csc_casprintf("%r.track", stitle),
                        csc_casprintf("%d", si));
            }
            CORD_fprintf(stderr, "^PLIP: Video track %r (%d) included\n", stitle, si);
        }
    }

    // Look for audio tracks
    int others = 0;
    bool usedThreads[nbStreams];
    pthread_t threads[nbStreams];
    for (int si = 0; si < nbStreams; si++) {
        CORD stype = codecType(streams, si);
        CORD stitle = title(streams, si);
        usedThreads[si] = false;

        if (!CORD_cmp(stype, "audio")) {
            fixTitle(&others, &stitle);

            // Ignore it if we must
            if (!csc_configBool(csc_configTree, "tracks.include", stitle)) {
                CORD_fprintf(stderr, "^PLIP: Audio track %r (%d) excluded\n", stitle, si);
                continue;
            }

            // Do the audio configuration
            CORD_fprintf(stderr, "^PLIP: Audio track %r (%d) included\n", stitle, si);
            if (dryRun)
                continue;
            struct AudioThread *at = GC_NEW(struct AudioThread);
            at->inputFile = inputFile;
            at->trackNo = si;
            at->title = stitle;
            if (GC_pthread_create(threads + si, NULL, audioThread, at) != 0)
                CRASH("pthread_create");
            usedThreads[si] = true;
        }
    }

    // Now wait for them
    for (int si = 0; si < nbStreams; si++) {
        if (usedThreads[si])
            pthread_join(threads[si], NULL);
    }

    return 0;
}
