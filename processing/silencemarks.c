/*
 * Copyright (c) 2022 Gregor Richards
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

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "arg.h"
#include "buffer.h"
#include "cscript.h"
#include "configfile.h"

static CORD ffmpeg = "ffmpeg";

BUFFER(charp, char *);

struct Mark {
    char op;
    char status;
    float val;
    char *line;
};

#define BUFSZ 4096

void usage(void);
struct Mark readMark(struct Mark prior);
void writeMark(struct Mark mark);

int main(int argc, char **argv)
{
    ARG_VARS;

    int audioCt = 0;
    struct Buffer_charp cl;
    const char *configFile = NULL;
    struct Mark marks[2];
    char buf[BUFSZ];
    float silenceStart, silenceEnd;

    csc_init(argv[0]);

    INIT_BUFFER(cl);
    WRITE_ONE_BUFFER(cl, NULL); /* To be ffmpeg */

    ARG_NEXT();
    while (argType) {
        ARG(h, help) {
            usage();
            exit(0);
        } else ARG(c, config) {
            configFile = arg;
        } else if (argType == ARG_VAL) {
            WRITE_ONE_BUFFER(cl, "-i");
            WRITE_ONE_BUFFER(cl, arg);
            audioCt++;
        } else {
            usage();
            exit(1);
        }
        ARG_NEXT();
    }

    csc_configInit(configFile);
    ffmpeg = csc_config("programs.ffmpeg");
    cl.buf[0] = CORD_to_char_star(ffmpeg);

    // Start reading marks from stdin
    marks[0].op = 'o';
    marks[0].status = 'o';
    marks[0].val = 0;
    marks[1] = readMark(marks[0]);

    // Make the rest of our ffmpeg command
    WRITE_ONE_BUFFER(cl, "-filter_complex");
    CORD filter = NULL, step;
    for (int i = 0; i < audioCt; i++) {
        step = csc_casprintf("[%d:a]", i);
        filter = CORD_cat(filter, step);
    }

    step = csc_casprintf("amix=%d,dynaudnorm,silencedetect=-25dB[aud]", audioCt);
    filter = CORD_cat(filter, step);
    WRITE_ONE_BUFFER(cl, CORD_to_char_star(filter));
    WRITE_ONE_BUFFER(cl, "-map");
    WRITE_ONE_BUFFER(cl, "[aud]");
    WRITE_ONE_BUFFER(cl, "-f");
    WRITE_ONE_BUFFER(cl, "null");
    WRITE_ONE_BUFFER(cl, "-");
    WRITE_ONE_BUFFER(cl, NULL);

    // And start ffmpeg
    int silPipeFd = csc_runp(-1, cl.buf);
    FILE *silPipe = fdopen(silPipeFd, "rb");
    buf[BUFSZ-1] = 0;

    // Read all the silence info
    while (fgets(buf, BUFSZ-1, silPipe)) {
        CORD *matches;
        matches = csc_match("silence_start: ([0-9\\.]*)", buf);
        if (matches) {
            silenceStart = atof(CORD_to_char_star(matches[1]));
            continue;
        }

        matches = csc_match("silence_end: ([0-9\\.]*)", buf);
        if (!matches)
            continue;

        silenceEnd = atof(CORD_to_char_star(matches[1]));

        // Make sure we're looking at the right gap
        while (marks[1].val < silenceEnd) {
            writeMark(marks[1]);
            marks[0] = marks[1];
            marks[1] = readMark(marks[0]);
        }
        if (marks[0].val >= silenceStart)
            continue;

        // Now account for the silence if applicable
        if (marks[0].status == 'i')
            printf("o%f\ni%f\n", silenceStart+0.5, silenceEnd-0.5);
    }

    // Finish up any remaining input marks
    while (marks[1].op != '_') {
        writeMark(marks[1]);
        marks[0] = marks[1];
        marks[1] = readMark(marks[0]);
    }

    return 0;
}

void usage()
{
    fprintf(stderr, "Use: plip-silencemarks <input audio> < marks > marks\n");
}

struct Mark readMark(struct Mark prior)
{
    char buf[BUFSZ];
    struct Mark ret;
    buf[BUFSZ-1] = 0;
    if (fgets(buf, BUFSZ-1, stdin)) {
        ret.op = buf[0];
        ret.val = atof(buf + 1);
        ret.line = GC_strdup(buf);
    } else {
        ret.op = '_';
        ret.val = FLT_MAX;
        ret.line = NULL;
    }

    // Determine the status
    switch (ret.op) {
        case 'i':
        case 'n':
            ret.status = 'i';
            break;

        case 'o':
        case 'f':
            ret.status = 'o';
            break;

        default:
            ret.status = prior.status;
    }

    return ret;
}

void writeMark(struct Mark mark)
{
    if (mark.line)
        CORD_printf("%r", mark.line);
    else
        printf("%c%f\n", mark.op, mark.val);
}
