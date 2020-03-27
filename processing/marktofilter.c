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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "arg.h"
#include "configfile.h"
#include "cscript.h"

#define BUFSZ 4096

char *getMarkLine(char *buf, int bufsz, FILE *stream);

int main(int argc, char **argv)
{
    ARG_VARS;

    csc_init(argv[0]);

    char buf[BUFSZ];
    FILE *inFile, *outFile;
    int currentIn = 0;
    double lastIn, prevLen;
    int i;
    int selections = 0;

    char *inFileNm = "out.mark";
    char *outFileNm = NULL;
    char *audio = NULL;
    char *video = NULL;
    char countRestarts = 0;
    unsigned long restartCount = 0;
    int chosenRestart = 0;
    int fps = 30;
    int arate = 48000;
    int akeep = 0;
    int adiscard = 0;
    double ffLen = 8;
    double minFFSpeed = 4;
    double maxFFPitch = INFINITY;
    char *ffFilter = "null";

    const char *configFile = NULL;
    ARG_NEXT();
    while (argType) {
        ARGNV(i, in-file, inFileNm)
        ARGNV(o, out-file, outFileNm)
        ARGL(count-restarts) {
            countRestarts = 1;
        } else ARG(r, chosen-restart) {
            ARG_GET();
            chosenRestart = atoi(arg) - 1;
        } else
        ARGNV(a, audio, audio)
        ARGNV(v, video, video)
        ARGV(k, audio-keep, akeep)
        ARGLV(audio-discard, adiscard)
        ARGN(c, config) {
            ARG_GET();
            configFile = arg;
            if (!strcmp(arg, "-"))
                configFile = NULL;
        } else ARGLN(fps) {
            ARG_GET();
            fps = atoi(arg);
        } else ARGLN(arate) {
            ARG_GET();
            arate = atoi(arg);
        } else {
            fprintf(stderr, "Unrecognized argument!\n");
            return 1;
        }
        ARG_NEXT();
    }

    /* Get config options */
    csc_configInit(configFile);
    double ffLenSet = csc_configDouble(csc_configTree, "marktofilter.fflen", NULL);
    if (ffLenSet != 0) ffLen = ffLenSet;
    double minFFSpeedSet = csc_configDouble(csc_configTree, "marktofilter.minffspeed", NULL);
    if (minFFSpeedSet != 0) minFFSpeed = minFFSpeedSet;
    double maxFFPitchSet = csc_configDouble(csc_configTree, "marktofilter.maxffpitch", NULL);
    maxFFPitch = maxFFPitchSet;
    if (maxFFPitch < 1) maxFFPitch = INFINITY;
    CORD ffFilterSet = csc_config("marktofilter.fffilter");
    if (ffFilterSet) ffFilter = CORD_to_char_star(ffFilterSet);

    /* open the files */
    inFile = fopen(inFileNm, "r");
    if (outFileNm) {
        outFile = fopen(outFileNm, "w");
        if (!outFile) {
            perror(outFileNm);
            exit(1);
        }
    } else {
        outFile = stdout;
    }

    /* start with the audio ready */
    if (audio)
        fprintf(outFile, "[%s]anull[aut];\n", audio);
    if (video)
        fprintf(outFile, "[%s]null[vit];\n", video);

    /* go step by step */
    lastIn = prevLen = 0;
    while (getMarkLine(buf, BUFSZ, inFile)) {
        double val = atof(buf + 1);
        switch (buf[0]) {
            case 'r':
                chosenRestart--;
                restartCount++;
                break;

            case 'i':
                currentIn = 1;
                lastIn = val;
                break;

            case 'f':
                /* abnormal out, into a fast-forward section */
            case 'o':
                /* normal out, do a trim and relocate */
                if (chosenRestart == 0) {
                    /* ffmpeg complains if you trim to length 0 */
                    if (val <= lastIn)
                        val = lastIn + 0.001;
                    if (audio) {
                        fprintf(outFile, "[aut]asplit[auu][aut];\n"
                               "[auu]atrim=%f:%f,asetpts=PTS-STARTPTS[au%d];\n",
                                           lastIn, val, selections);
                    }
                    if (video) {
                        fprintf(outFile, "[vit]split[viu][vit];\n"
                               "[viu]trim=%f:%f,setpts=PTS-STARTPTS[vi%d];\n",
                                           lastIn, val, selections);
                    }
                    selections++;
                    prevLen += (val - lastIn);
                    currentIn = 0;
                    lastIn = val;
                }
                break;

            case 'n':
                if (chosenRestart == 0) {
                    /* out for the fast-forward, in for normal */
                    double len, outLen, aspeedup, vspeedup, tempoup;
                    len = val - lastIn;
                    if (len <= 0)
                        len = 0.001;
                    if (len <= ffLen * minFFSpeed) {
                        /* We're fast-forwarding to less than the maximum FF
                         * length, so do the minimum FF speed */
                        vspeedup = minFFSpeed;
                        outLen = len / minFFSpeed;
                    } else {
                        /* Normal fast-forward */
                        vspeedup = len / ffLen;
                        outLen = ffLen;
                    }

                    /* Adjust the tempo component so we don't go over the max pitch */
                    aspeedup = vspeedup;
                    tempoup = 1;
                    if (aspeedup > maxFFPitch) {
                        tempoup = aspeedup / maxFFPitch;
                        aspeedup = maxFFPitch;
                    }

                    /* now make the commands */
                    if (audio) {
                        if (!adiscard) fprintf(outFile, "[aut]asplit[auu][aut];\n");
                        if (adiscard) {
                            /* actually don't want anything here! */
                            fprintf(outFile, "aevalsrc=0,atrim=0:%f[au%d];\n",
                                                       outLen, selections);
                        } else if (akeep) {
                            /* just keep an appropriate duration of audio */
                            fprintf(outFile, "[auu]atrim=%f:%f,asetpts=PTS-STARTPTS[au%d];\n",
                                               lastIn, lastIn + outLen, selections);
                        } else {
                            /* speed it up */
                            fprintf(outFile, "[auu]atrim=%f:%f,asetpts=PTS-STARTPTS,aresample=%d,asetrate=%f,aresample=%d",
                                               lastIn, val+len,                               arate, arate*aspeedup, arate);
                            while (tempoup > 2) {
                                fprintf(outFile, ",atempo=2");
                                tempoup /= 2;
                            }
                            if (tempoup != 1) fprintf(outFile, ",atempo=%f", tempoup);
                            fprintf(outFile, ",aresample=%d,atrim=0:%f[au%d];\n",
                                               arate, outLen, selections);
                        }
                    }

                    if (video) {
                        fprintf(outFile, "[vit]split[viu][vit];\n"
                               "[viu]trim=%f:%f,setpts=(PTS-STARTPTS)/%f,\n"
                               "     fps=%d:start_time=0,trim=0:%f,%s[vi%d];\n",
                                          lastIn, val+len,            vspeedup,
                                         fps,                   outLen, ffFilter, selections);
                    }

                    selections++;
                    currentIn = 1;
                    lastIn = val;
                    prevLen += outLen;
                    break;
                }

            case 'm':
                if (chosenRestart == 0 && !countRestarts && !audio && !video) {
                    /* a mark, write it out */
                    long s, m, h;
                    double mark = prevLen;
                    if (currentIn)
                        mark += val - lastIn;
                    s = (long) mark;
                    m = s / 60;
                    h = m / 60;

                    s %= 60;
                    m %= 60;

                    fprintf(outFile, "m ");
                    if (h) fprintf(outFile, "%ld:", h);
                    if (h || m) fprintf(outFile, "%02ld:", m);
                    fprintf(outFile, "%02ld\n", s);
                }
                break;
        }
    }

    /* count restarts */
    if (countRestarts)
        fprintf(outFile, "%d\n", restartCount);

    /* now bring together all our audio */
    if (audio) {
        fprintf(outFile, "[aut]atrim=0:0[aut];\n[aut]");
        for (i = 0; i < selections; i++)
            fprintf(outFile, "[au%d]", i);
        fprintf(outFile, "concat=n=%d:v=0:a=1[aud]%s\n", selections + 1, video ? ";" : "");
    }

    /* and all our video */
    if (video) {
        fprintf(outFile, "[vit]trim=0:0[vit];\n[vit]");
        for (i = 0; i < selections; i++)
            fprintf(outFile, "[vi%d]", i);
        fprintf(outFile, "concat=n=%d:v=1:a=0,fps=%d:start_time=0[vid]\n", selections + 1, fps);
    }

    if (inFile) fclose(inFile);
    if (outFile != stdout) fclose(outFile);

    return 0;
}

/* Get a line from the marks file, which might be NULL */
char *getMarkLine(char *buf, int bufsz, FILE *stream)
{
    if (stream)
        return fgets(buf, bufsz, stream);

    static int counter = 0;

    switch (counter++) {
        case 0:
            strncpy(buf, "i0\n", bufsz);
            break;

        case 1:
            strncpy(buf, "o86400\n", bufsz);
            break;

        default:
            return NULL;
    }

    return buf;
}
