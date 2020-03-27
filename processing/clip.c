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

#include <string.h>
#include <unistd.h>

#include "arg.h"
#include "buffer.h"
#include "cscript.h"
#include "configfile.h"

static const char *quote = "^[^\"]*\"(.*)\"$";
static const char *equals = "^[^=]*=(.*)$";
static CORD ffmpeg = "ffmpeg";
static CORD ffprobe = "ffprobe";
static CORD aiformat = "flac";

BUFFER(charp, char *);

// Get the stream info from a file
CORD streams(CORD inputFile)
{
    CORD ret = NULL;
    csc_runl(CSC_STDOUT, &ret,
        ffprobe, "-print_format", "flat", "-show_streams", inputFile, NULL);
    return ret;
}

// Get the FPS of the first stream (FIXME? Only gives 60 or 30)
double fps(CORD streams)
{
    CORD *fps = csc_grep("^streams\\.stream\\.[0-9]*\\.r_frame_rate", streams);
    if (!fps) return 30;
    for (size_t fi = 0; fps[fi]; fi++) {
        if (csc_match("0/0", fps[fi])) continue;
        fps = csc_match(quote, fps[fi]);
        if (!fps || !fps[1]) break;
        if (atof(CORD_to_char_star(fps[1])) >= 40)
            return 60;
        else
            return 30;
    }
    return 30;
}

// Get the video width from its stream info
int vwidth(CORD streams)
{
    CORD *w = csc_grep("^streams\\.stream\\.[0-9]*\\.width", streams);
    if (!w || !w[0]) return 0;
    w = csc_match(equals, w[0]);
    if (!w || !w[1]) return 0;
    return atoi(CORD_to_char_star(w[1]));
}

// Get the video height from its stream info
int vheight(CORD streams)
{
    CORD *h = csc_grep("^streams\\.stream\\.[0-9]*\\.height", streams);
    if (!h || !h[0]) return 0;
    h = csc_match(equals, h[0]);
    if (!h || !h[1]) return 0;
    return atoi(CORD_to_char_star(h[1]));
}

// Get the video size (wxh) from its stream info
CORD vsize(CORD streams)
{
    return csc_casprintf("%dx%d", vwidth(streams), vheight(streams));
}

void usage()
{
    fprintf(stderr,
        "Use: plip-clip [-v] [-c] [-C|--config <config file>] [-i] <input file> [[-m] <marks file>]\n"
        "Options:\n"
        "\t-v|--verbose: Verbose mode\n"
        "\t-c|--cleanup: Clean up clipped files instead of clipping.\n\n");
}

int main(int argc, char **argv)
{
    ARG_VARS;
    bool cleanup = false;

    csc_init(argv[0]);

    ARG_NEXT();
    const char *inputFile = NULL, *marksFile = NULL, *configFile = NULL;
    while (argType) {
        ARG(h, help) {
            usage();
            exit(0);
        } else ARG(v, verbose) {
            csc_verbose = true;
        } else ARG(c, cleanup) {
            cleanup = true;
        } else ARGN(i, input-file) {
            ARG_GET();
            inputFile = arg;
        } else ARGN(m, marks-file) {
            ARG_GET();
            marksFile = arg;
        } else ARGN(C, config) {
            ARG_GET();
            configFile = arg;
            if (!strcmp(arg, "-"))
                configFile = NULL;
        } else if (argType == ARG_VAL) {
            if (!inputFile)
                inputFile = arg;
            else if (!marksFile)
                marksFile = arg;
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

    csc_configInit(configFile);
    if (!configFile) configFile = "-";
    ffmpeg = csc_config("programs.ffmpeg");
    ffprobe = csc_config("programs.ffprobe");
    aiformat = csc_config("formats.aiformat");

    if (inputFile && !marksFile) {
        // Try modifying the input file name into a marks file name
        CORD marksFileMod = inputFile;
        char *right = strrchr(inputFile, '.');
        if (right)
            marksFileMod = CORD_substr(marksFileMod, 0, right - inputFile);
        marksFileMod = CORD_cat(marksFileMod, ".mark");
        marksFile = CORD_to_char_star(marksFileMod);
    }

    if (!inputFile || !marksFile) {
        usage();
        exit(1);
    }

    // ulimit -v $(( 4 * 1024 * 1024 ))

    // Figure out how many clip steps we need to perform
    CORD resetCountStr;
    if (csc_runl(CSC_STDOUT, &resetCountStr,
        "plip-marktofilter", "-c", configFile, "-i", marksFile, "--count-restarts", NULL) < 0)
        CRASH("marktofilter");
    int resetCount = atoi(CORD_to_char_star(resetCountStr));

    for (int resetNum = 0; resetNum <= resetCount; resetNum++) {
        CORD resetSuffix = NULL;
        CORD resetNumStr = csc_casprintf("%d", resetNum + 1);
        if (resetCount > 0)
            resetSuffix = resetNumStr;

        // Make our human-readable marks file
        CORD marksOut = csc_casprintf("marks%r.txt", resetSuffix);
        csc_runl(0, NULL,
            "plip-marktofilter", "-c", configFile, "-i", marksFile, "-r", resetNumStr, "-o", marksOut, NULL);

        // And our not-so-human-readable mark filters
        CORD video30Marks;
        if (csc_runl(CSC_STDOUT, &video30Marks,
            "plip-marktofilter", "-c", configFile, "-i", marksFile, "-r", resetNumStr, "-v", "vid", NULL) < 0)
            CRASH("marktofilter");
        CORD video60Marks;
        if (csc_runl(CSC_STDOUT, &video60Marks,
            "plip-marktofilter", "-c", configFile, "-i", marksFile, "-r", resetNumStr, "--fps", "60", "-v", "vid", NULL) < 0)
            CRASH("marktofilter");
        CORD audioMarks;
        if (csc_runl(CSC_STDOUT, &audioMarks,
            "plip-marktofilter", "-c", configFile, "-i", marksFile, "-r", resetNumStr, "-a", "0:a", NULL) < 0)
            CRASH("marktofilter");
        CORD voiceMarks;
        if (csc_runl(CSC_STDOUT, &voiceMarks,
            "plip-marktofilter", "-c", configFile, "-i", marksFile, "-r", resetNumStr, "-a", "0:a", "-k", NULL) < 0)
            CRASH("marktofilter");
        CORD discardMarks;
        if (csc_runl(CSC_STDOUT, &discardMarks,
            "plip-marktofilter", "-c", configFile, "-i", marksFile, "-r", resetNumStr, "-a", "0:a", "--audio-discard", NULL) < 0)
            CRASH("marktofilter");

        // Process the video tracks
        CORD *vidTracks = csc_glob("*.track");
        for (size_t vi = 0; vidTracks[vi]; vi++) {
            CORD vidTrack = vidTracks[vi];

            // Get the base
            CORD *trackParts = csc_match("^(.*)\\.track$", vidTrack);
            if (!trackParts || !trackParts[1]) continue;
            CORD trackBase = trackParts[1];

            // Choose our contextual format info
            CORD vformat = csc_configRead(csc_configTree, "formats.vformat", vidTrack, NULL);
            CORD vcodec = csc_configRead(csc_configTree, "formats.vcodec", vidTrack, NULL);
            CORD vcrf = csc_configRead(csc_configTree, "formats.vcrf", vidTrack, NULL);
            CORD vbr = csc_configRead(csc_configTree, "formats.vbr", vidTrack, NULL);
            CORD vflags = csc_configRead(csc_configTree, "formats.vflags", vidTrack, NULL);

            // Get the output name
            CORD trackOut = csc_casprintf("%r%r.%r", trackBase, resetSuffix, vformat);
            int trackNum = atoi(CORD_to_char_star(csc_readFile(vidTrack)));
            CORD trackMap = csc_casprintf("0:%d", trackNum);

            // Clean up if that's what we were requested to do
            if (cleanup) {
                CORD_fprintf(stderr, "^PLIP: Cleanup: %r\n", CORD_to_char_star(trackOut));
                unlink(CORD_to_char_star(trackOut));
                continue;
            }

            // If we've already processed this, never mind!
            if (csc_fileExists(trackOut)) {
                CORD_fprintf(stderr, "^PLIP: Video track %r already clipped (%r).\n", trackBase, trackOut);
                continue;
            }

            // Figure out the FPS (FIXME: Multiple video streams)
            CORD vStreams = streams(inputFile);
            int vFps = fps(vStreams);

            // If we need to, deinterlace
            CORD iFilters = "null";
            if (csc_match("iv$", trackBase))
                iFilters = "yadif=mode=send_field_nospatial:parity=tff,mcdeint=parity=tff";

            // If we're bypassing normal video processing, call a script
            CORD videoBypass = csc_configRead(csc_configTree, "steps.videobypass", trackBase, NULL);
            if (videoBypass) {
                csc_runl(0, NULL,
                    videoBypass,
                    inputFile,
                    csc_casprintf("[%r]null[vid];%r", trackMap, (vFps==60)?video60Marks:video30Marks),
                    trackOut,
                    NULL);

            } else {
                // And get any extra filters
                CORD exFilters = csc_configRead(csc_configTree, "filters.video", trackBase, NULL);

                // Make the command line
                struct Buffer_charp cl;
                INIT_BUFFER(cl);
#define W(x) WRITE_ONE_BUFFER(cl, x)
                W(CORD_to_char_star(ffmpeg));
                W("-nostdin");
                W("-copyts");
                W("-i");
                W(CORD_to_char_star(inputFile));
                W("-filter_complex");
                W(CORD_to_char_star(
                    csc_casprintf("[%r]null[vid];%r;[vid]%r,%r[vid]", trackMap, (vFps==60)?video60Marks:video30Marks, iFilters, exFilters)
                ));
                W("-map");
                W("[vid]");
                if (!CORD_cmp(vflags, NULL)) {
                    // Default: Just use vcrf and/or vbr, assume -threads and -preset for x264
                    W("-c:v");
                    W(CORD_to_char_star(vcodec));
                    W("-threads");
                    W("0");
                    W("-preset");
                    W("ultrafast");
                    if (CORD_cmp(vcrf, NULL)) {
                        W("-crf");
                        W(CORD_to_char_star(vcrf));
                    } else if (CORD_cmp(vbr, NULL)) {
                        W("-b:v");
                        W(CORD_to_char_star(vbr));
                    }
                } else {
                    // User-supplied args, split them
                    char *abuf = CORD_to_char_star(vflags);
                    char *lasts = NULL, *cur;
                    cur = strtok_r(abuf, " \t\r\n", &lasts);
                    while (cur) {
                        if (strcmp(cur, ""))
                            W(cur);
                        cur = strtok_r(NULL, " \t\r\n", &lasts);
                    }
                }
                W(CORD_to_char_star(trackOut));
                W(NULL);
#undef W

                // Now clip the video
                CORD_fprintf(stderr, "^PLIP: Clipping video track %r (%r).\n", trackBase, trackOut);
                csc_run(0, NULL, cl.buf);

                FREE_BUFFER(cl);
            }
        }

        // Clip all the audio files
        CORD audioGlob = csc_casprintf("*-proc.%r", aiformat);
        CORD *audioFiles = csc_glob(audioGlob);
        for (size_t ai = 0; audioFiles[ai]; ai++) {
            CORD audioFile = audioFiles[ai];

            // Skip our noise files
            if (csc_match("^noise-", audioFile)) continue;

            // Get the track name
            CORD audioRE = csc_casprintf("^(.*)-proc\\.%r$", aiformat);
            CORD *audioParts = csc_match(audioRE, audioFile);
            if (!audioParts || !audioParts[1]) continue;
            CORD audioBase = audioParts[1];

            // Figure out which marks to use
            CORD marks = audioMarks;
            CORD markSet = csc_configRead(csc_configTree, "filters.ffclip", audioBase, NULL);
            if (!CORD_cmp(markSet, "keep"))
                marks = voiceMarks;
            else if (!CORD_cmp(markSet, "discard"))
                marks = discardMarks;

            // Figure out the audio format and codec to use
            CORD aformat = csc_configRead(csc_configTree, "formats.aformat", audioBase, NULL);
            CORD acodec = csc_configRead(csc_configTree, "formats.acodec", audioBase, NULL);
            CORD abr = csc_configRead(csc_configTree, "formats.abr", audioBase, NULL);

            // Figure out the name to generate
            CORD audioOut = csc_casprintf("%r%r.%r", audioBase, resetSuffix, aformat);

            // Clean up if that's what we were requested to do
            if (cleanup) {
                CORD_fprintf(stderr, "^PLIP: Cleanup: %r\n", CORD_to_char_star(audioOut));
                unlink(CORD_to_char_star(audioOut));
                continue;
            }

            // Don't generate it if we already did
            if (csc_fileExists(audioOut)) {
                CORD_fprintf(stderr, "^PLIP: Audio track %r already clipped (%r).\n", audioBase, audioOut);
                continue;
            }

            // Make the arguments
            struct Buffer_charp args;
            INIT_BUFFER(args);
#define A(x) WRITE_ONE_BUFFER(args, x)
            A(CORD_to_char_star(ffmpeg));
            A("-nostdin");
            A("-i");
            A(CORD_to_char_star(audioFile));
            A("-filter_complex");
            A(CORD_to_char_star(marks));
            A("-map");
            A("[aud]");
            A("-c:a");
            A(CORD_to_char_star(acodec));
            if (CORD_cmp(abr, NULL)) {
                A("-b:a");
                A(CORD_to_char_star(abr));
            }
            A(CORD_to_char_star(audioOut));
            A(NULL);
#undef A

            // And finally, clip it
            CORD_fprintf(stderr, "^PLIP: Clipping audio track %r (%r).\n", audioBase, audioOut);
            csc_run(0, NULL, args.buf);
        }

    }

    return 0;
}
