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
#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "arg.h"
#include "cscript.h"
#include "configfile.h"

static CORD ffmpeg = "ffmpeg";
static CORD iformat = "flac";
static CORD icodec = "flac";

#ifdef _WIN32
#define DEVNULL "NUL"
#else
#define DEVNULL "/dev/null"
#endif

// Figure out the normalization level for a file
static double normlevel(CORD file, double target /* def: -18 */)
{
    CORD summary;
    double loudness;
    csc_runl(CSC_STDERR, &summary,
        ffmpeg, "-i", file, "-af", "loudnorm=print_format=summary", "-f",
        "wav", "-y", DEVNULL, NULL);

    CORD *sintigrated = csc_grep("^Input Integrated:", summary);
    if (!sintigrated || !sintigrated[0])
        return 0;
    sintigrated = csc_match(": (.*)$", sintigrated[0]);
    if (!sintigrated || !sintigrated[1])
        return 0;

    loudness = atof(CORD_to_char_star(sintigrated[1]));
    if (!isnormal(loudness)) loudness = target;
    return target - loudness;
}

// Process an audio file
struct AprocThread {
    CORD input, base;
    bool deleteAfter; // delete when we're done
    pthread_barrier_t *start; // wait for this before starting
    pthread_rwlock_t *wlock; // lock for writing our file
    CSC_HashTable *threadTable; // all other rwlocks, for dependencies
};
void *aproc(void *vat)
{
    struct AprocThread *at = vat;

    CORD input = at->input;
    CORD base = at->base;

    // Get the writer lock
    pthread_rwlock_wrlock(at->wlock);

    // Wait until we're ready
    pthread_barrier_wait(at->start);

    // And process
    input = csc_absolute(input);

    CORD noiserFile = csc_absolute(csc_casprintf("%r-noiser.%r", base, iformat));
    CORD noiseFile = csc_absolute(csc_casprintf("%r-noise.f32", base));
    CORD outFile = csc_absolute(csc_casprintf("%r-proc.%r", base, iformat));
    CORD noiser = csc_configRead(csc_configTree, "steps.noiser", base, NULL);
    bool noiseLearn = csc_configBool(csc_configTree, "steps.noiserlearn", base);

    // If we're already done, we're already done!
    if (csc_fileExists(outFile)) {
        CORD_fprintf(stderr, "^PLIP: %r already processed, skipping.\n", base);
        pthread_rwlock_unlock(at->wlock);
        return NULL;
    }

    // Do noise reduction if asked
    if (CORD_cmp(noiser, NULL)) {
        char *program = CORD_to_char_star(csc_casprintf("plip-%rdenoise", noiser));
        char *format = "s16le";
        if (!CORD_cmp(noiser, "noiserepellent"))
            format = "f32le";

        // Find noise if needed
        if (noiseLearn && !csc_fileExists(noiseFile)) {
#ifdef _WIN32
            // Windows ffmpeg doesn't pipeline well
            char *inter = CORD_to_char_star(csc_absolute(csc_casprintf("%r-noise1.raw", base)));
            csc_runl(0, NULL,
                ffmpeg,
                "-i", input,
                "-f", "f32le", "-ac", "2", "-ar", "48000",
                inter, NULL);
            int aud2 = csc_runpl(-1,
                "plip-findnoise",
                "-i", inter,
                "-o", CORD_to_char_star(noiseFile),
                "2", NULL);

#else
            int aud1 = csc_runpl(-1,
                ffmpeg,
                "-i", input,
                "-f", "f32le", "-ac", "2", "-ar", "48000",
                "-", NULL);
            int aud2 = csc_runpl(aud1,
                "plip-findnoise",
                "-o", CORD_to_char_star(noiseFile),
                "2", NULL);

#endif
            csc_wait(aud2);

#ifdef _WIN32
            unlink(inter);
#endif
        }

        // Perform noise reduction
        if (!csc_fileExists(noiserFile)) {
            int noiseRed;

            // Set up the pipeline
#ifdef _WIN32
            // Windows ffmpeg doesn't pipeline well
            char *inter = CORD_to_char_star(csc_absolute(csc_casprintf("%r-noiser1.raw", base)));
            csc_runl(0, NULL,
                ffmpeg,
                "-i", input,
                "-f", format, "-ac", "2", "-ar", "48000",
                inter, NULL);
            if (noiseLearn) {
                noiseRed = csc_runpl(-1,
                    program,
                    "-i", inter,
                    "-l", noiseFile,
                    "2", NULL);
            } else {
                noiseRed = csc_runpl(-1,
                    program,
                    "-i", inter,
                    "2", NULL);
            }

#else
            int aud1 = csc_runpl(-1,
                ffmpeg,
                "-i", input,
                "-f", format, "-ac", "2", "-ar", "48000",
                "-", NULL);
            if (noiseLearn) {
                noiseRed = csc_runpl(aud1,
                    program,
                    "-l", noiseFile,
                    "2", NULL);
            } else {
                noiseRed = csc_runpl(aud1,
                    program, "2", NULL);
            }
#endif

            int aud2 = csc_runpl(noiseRed,
                ffmpeg,
                "-f", format, "-ac", "2", "-ar", "48000", "-i", "-",
                "-c:a", icodec,
                CORD_to_char_star(noiserFile), NULL);
            csc_wait(aud2);

#ifdef _WIN32
            unlink(inter);
#endif
        }

        // There's no reason to keep the learning file around
        if (noiseLearn)
            unlink(CORD_to_char_star(noiseFile));

    } else {
        unlink(CORD_to_char_star(noiserFile));
        csc_ln(input, noiserFile);

    }

    // Then perform all requested processing steps
    CORD lastFile = noiserFile;
    int steps = csc_configInt(csc_configTree, "steps.aproc", base);
    if (csc_verbose)
        fprintf(stderr, "^PLIP: %r: %d audio processing steps\n", base, steps);
    for (int si = 1; si <= steps; si++) {
        CORD filterName = csc_configRead(csc_configTree, csc_casprintf("filters.aproc%d", si), base, NULL);
        double desiredLevel = csc_configDouble(csc_configTree, csc_casprintf("filters.alevel%d", si), base);

        if (csc_verbose)
            CORD_fprintf(stderr, "^PLIP: %r: Audio processing step %d: %r\n", base, si, filterName);

        // What's our output?
        CORD nextFile;
        if (si == steps)
            nextFile = outFile;
        else
            nextFile = csc_casprintf("%r-aproc%d.%r", base, si, iformat);

        // If this is a null step, skip it
        if (!CORD_cmp(filterName, "null")) {
            if (si == steps) {
                // But still need to convert
                csc_runl(0, NULL,
                    ffmpeg, "-i", lastFile, "-c:a", icodec, nextFile, NULL);
                unlink(CORD_to_char_star(lastFile));

            } else {
                // Just rename it
                rename(CORD_to_char_star(lastFile), CORD_to_char_star(nextFile));

            }

            lastFile = nextFile;
            continue;
        }

        CSC_HashTable *filterVars = csc_newHashTable();

        // Figure out the necessary leveling if requested
        if (desiredLevel != 0.0) {
            double n = normlevel(lastFile, desiredLevel);
            csc_htAdd(filterVars, "level", (void *) csc_casprintf("%f", n));
        }

        // Get any dependencies the filter has
        CORD *filterDeps = csc_lines(csc_configRead(csc_configTree, csc_casprintf("filters.%rdeps", filterName), base, NULL));
        for (size_t fdi = 0; filterDeps[fdi]; fdi++) {
            // Wait for it
            CORD dep = filterDeps[fdi];
            CORD depOut = csc_casprintf("%r-proc.%r", dep, iformat);
            pthread_rwlock_t *otherWlock = csc_htGet(at->threadTable, dep);
            if (otherWlock) {
                pthread_rwlock_rdlock(otherWlock);
                pthread_rwlock_unlock(otherWlock);
            }
            csc_htAdd(filterVars, csc_casprintf("dep%d", (int) (fdi+1)), (void *) depOut);
        }

        // Get the filter
        CORD filter = csc_configRead(csc_configTree, csc_casprintf("filters.%r", filterName), base, filterVars);

        // And run it
        csc_runl(0, NULL,
            ffmpeg, "-i", lastFile,
            "-filter_complex", csc_casprintf("[0:a]%r[aud]", filter),
            "-map", "[aud]",
            "-c:a", icodec,
            "-y", nextFile, NULL);

        unlink(CORD_to_char_star(lastFile));
        lastFile = nextFile;
    }

    // Clean up
    if (at->deleteAfter)
        unlink(CORD_to_char_star(input));

    // And mark ourself as done
    pthread_rwlock_unlock(at->wlock);
}

void usage()
{
    fprintf(stderr, "Use: plip-aproc [-c|--config <config file>] [-v]\n\n");
}

int main(int argc, char **argv)
{
    ARG_VARS;

    csc_init(argv[0]);

    const char *configFile = NULL;
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
        } else {
            usage();
            exit(1);
        }
        ARG_NEXT();
    }

    csc_configInit(configFile);
    ffmpeg = csc_config("programs.ffmpeg");
    iformat = csc_config("formats.aiformat");
    icodec = csc_config("formats.aicodec");

    size_t rfi;
    CORD rawGlob = CORD_cat("*-raw.", iformat);
    CORD *rawFiles = csc_glob(rawGlob);
    CORD *syncFiles = csc_glob("*-sync.flac");

    if (syncFiles && syncFiles[0]) {
        // Combine them into one list
        size_t rfCount, sfCount;
        for (rfCount = 0; rawFiles[rfCount]; rfCount++);
        for (sfCount = 0; syncFiles[sfCount]; sfCount++);
        CORD *allFiles = GC_MALLOC((rfCount+sfCount+1) * sizeof(CORD));
        for (rfi = 0; rawFiles[rfi]; rfi++) {
            allFiles[rfi] = rawFiles[rfi];
        }
        for (size_t sfi = 0; syncFiles[sfi]; sfi++) {
            allFiles[rfi++] = syncFiles[sfi];
        }
        allFiles[rfi] = NULL;
        rawFiles = allFiles;
    }

    // Prepare our thread data
    CSC_HashTable *threadTable = csc_newHashTable();

    // Count the threads
    for (rfi = 0; rawFiles[rfi]; rfi++);
    pthread_barrier_t start;
    pthread_barrier_init(&start, NULL, (unsigned) rfi);
    pthread_t threads[rfi];

    // Run the aprocs
    for (rfi = 0; rawFiles[rfi]; rfi++) {
        // Figure out the names
        CORD input = rawFiles[rfi];
        bool deleteAfter = true;
        CORD stripRE = csc_casprintf("^(.*)-raw\\.%r$", iformat);
        CORD *stripStep = csc_match(stripRE, input);
        if (!stripStep || !stripStep[1]) {
            deleteAfter = false;
            stripStep = csc_match("^(.*)-sync\\.flac$", input);
            if (!stripStep || !stripStep[1])
                continue;
        }
        CORD base = stripStep[1];
        CORD output = csc_casprintf("%r-proc.%r", base, iformat);

        // Initialize the lock
        pthread_rwlock_t *wlock = GC_NEW(pthread_rwlock_t);
        pthread_rwlock_init(wlock, NULL);
        csc_htAdd(threadTable, base, wlock);

        // And make the thread
        struct AprocThread *at = GC_NEW(struct AprocThread);
        at->input = input;
        at->base = base;
        at->deleteAfter = deleteAfter;
        at->start = &start;
        at->wlock = wlock;
        at->threadTable = threadTable;
        if (GC_pthread_create(threads + rfi, NULL, aproc, at) != 0)
            CRASH("pthread_create");
    }

    // And wait for the threads
    for (rfi = 0; rawFiles[rfi]; rfi++) {
        pthread_join(threads[rfi], NULL);
    }

    return 0;
}
