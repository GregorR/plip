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

#include "cscript.h"

#ifdef _WIN32
#define PATH_SEP ";"
#define EXE_EXT ".exe"
#else
#define PATH_SEP ":"
#define EXE_EXT ""
#endif

int main(int argc, char **argv)
{
    csc_init(argv[0]);

    CORD target = NULL;
    char *bin;

    if (strchr(argv[0], CSC_DIRSEP[0])) {
        // We were called with an absolute path, so run relative to that absolute path
        target = csc_absolute(csc_dirname(argv[0]));

    } else {
        // Get the path
        char *path = GC_strdup(getenv("PATH"));

        // Go element-by-element
        char *lasts = NULL, *cur;
        cur = strtok_r(path, PATH_SEP, &lasts);
        while (cur) {
            char *check = CORD_to_char_star(csc_casprintf("%r%splip-gui%s", cur, CSC_DIRSEP, EXE_EXT));
            if (!access(check, X_OK)) {
                // Found it!
                target = cur;
                break;
            }

            cur = strtok_r(NULL, PATH_SEP, &lasts);
        }

        if (!target) {
            fprintf(stderr, "plip-gui not found!\n");
            return 1;
        }

    }

    // The target is just the directory we found ourselves in. Look for the GUI.
    bin = CORD_to_char_star(csc_casprintf("%r%sgui%splip-gui%s",
        target, CSC_DIRSEP, CSC_DIRSEP, EXE_EXT));
    if (access(bin, X_OK)) {
        // Try in the installation path
        bin = CORD_to_char_star(csc_casprintf("%r%s..%slib%splip-gui%splip-gui%s",
            target, CSC_DIRSEP, CSC_DIRSEP, CSC_DIRSEP, CSC_DIRSEP, EXE_EXT));
        if (access(bin, X_OK)) {
            fprintf(stderr, "plip-gui not found!\n");
            return 1;
        }
    }

    argv[0] = bin;
    execv(argv[0], argv);
    perror(argv[0]);
    return 1;
}
