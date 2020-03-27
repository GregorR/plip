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
#include <sys/types.h>

#include "buffer.h"

BUFFER(short, short);

#define SRATE 48000
#define CHANS 2
#define CSRATE (CHANS*SRATE)
#define SILENT_SEGMENT (CSRATE/100)

int main()
{
    struct Buffer_short samples;
    unsigned int curMinLoc = 0;
    unsigned long long curVal = 0, curMinVal = 0;
    size_t i, j, rangeMin, rangeMax;

    /* read in the file */
    INIT_BUFFER(samples);
    {
        size_t rd;
        while ((rd = fread(BUFFER_END(samples), 1, BUFFER_SPACE(samples), (stdin))) > 0) {
            STEP_BUFFER(samples, rd);
            if (BUFFER_SPACE(samples) <= 0) {
                short *newBuf = realloc(samples.buf, 2 * samples.bufsz * sizeof(short));
                if (!newBuf)
                    break;
                samples.bufsz *= 2;
                samples.buf = newBuf;
            }
        }
    }

    /* only look at the middle 1/3rd */
    rangeMin = samples.bufused / 3 / 2 * 2;
    rangeMax = rangeMin * 2;

    /* if the range is too small, just output emptiness */
    if (rangeMax - rangeMin < CSRATE) {
        while (samples.bufsz < CSRATE) EXPAND_BUFFER(samples);
        memset(samples.buf, 0, CSRATE*sizeof(short));
        fwrite(samples.buf, sizeof(short), CSRATE, stdout);
        return 0;
    }

    /* invalidate any streaks of complete silence */
    for (i = rangeMin; i < rangeMax; i += 2) {
        if (samples.buf[i] == 0 && samples.buf[i+1] == 0) {
            /* silence! Is it 960 samples (.01 seconds) long? */
            for (j = i + 2; j < i + SILENT_SEGMENT; j += 2) {
                if (samples.buf[j] != 0 || samples.buf[j+1] != 0) break;
            }
            if (j == i + SILENT_SEGMENT) {
                /* max it out */
                for (j--; j >= i; j--)
                    samples.buf[j] = 32767;
                i += SILENT_SEGMENT;
            } else {
                i = j - 2;
            }
        }
    }

    /* initialize the first second */
    for (i = rangeMin; i < rangeMax && i < rangeMin + CSRATE; i += 2) {
        curVal += abs(samples.buf[i]);
        curVal += abs(samples.buf[i+1]);
    }
    curMinLoc = i;
    curMinVal = curVal;

    /* then search for the best second */
    for (i = rangeMin + CSRATE; i < rangeMax; i += 2) {
        /* remove the start */
        curVal -= abs(samples.buf[i-CSRATE]);
        curVal -= abs(samples.buf[i-CSRATE+1]);

        /* add the current sample */
        curVal += abs(samples.buf[i]);
        curVal += abs(samples.buf[i+1]);

        /* keep it if it's the quietest */
        if (curVal < curMinVal) {
            curMinLoc = i;
            curMinVal = curVal;
        }
    }

    /* and output it */
    curMinLoc -= CSRATE;
    fwrite(samples.buf + curMinLoc, sizeof(short), CSRATE, stdout);

    return 0;
}
