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

#include "cscript.h"
#include "configfile.h"

#include "defconfig.h"

// An arbitrary recursion limit
#define MAX_CONFIG_DEPTH 17

// A configuration directive is a list of conditional overrides/extensions
typedef struct Directive_ {
    struct Directive_ *next;
    CORD condition;
    bool extension;
    bool left;
    CORD value;
} Directive;

static bool iswhite(char c)
{
    return (c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\v');
}

static bool isid(char c)
{
    return ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '_');
}

// Our main configuration
CSC_Config *csc_configTree;

// Extend a config with a specified file
void csc_extendConfig(CSC_Config *config, CORD file)
{
    CORD input, *lines;
    unsigned char *begin;

    if (file) {
        FILE *fh = fopen(CORD_to_char_star(file), "rb");
        if (!fh)
            return;
        if (csc_verbose)
            fprintf(stderr, "^CONFIG: File %s\n", CORD_to_char_star(file));
        input = CORD_from_file(fh);

        // Check for stupid files because of stupid Windows
        begin = (unsigned char *) CORD_to_char_star(CORD_substr(input, 0, 2));
        if (begin[0] == 0xFF || begin[1] == 0xFF) {
            fprintf(stderr, "^CONFIG: Unsupported UTF-16/UCS-2 config file. Please use UTF-8 or ASCII.\n");
            exit(1);
        }

        lines = csc_lines(input);
    } else {
        if (csc_verbose)
            fprintf(stderr, "^CONFIG: Default\n");
        lines = csc_lines(defConfig);
    }

    // Now extract the directives
    CORD prefix = NULL;
    for (size_t li = 0; lines[li]; li++) {
        CORD line = lines[li];
        size_t llen = CORD_len(line);

        // Add the next line if it asks
        while (llen &&
               CORD_fetch(line, llen-1) == '\\' &&
               lines[li+1]) {
            // Strip off the backslash
            line = CORD_substr(line, 0, llen-1);

            // And add the next line
            line = CORD_cat(line, lines[++li]);
            llen = CORD_len(line);
        }

        char *cline = CORD_to_char_star(line);
        size_t lidx, eidx;

        // Start parsing
        lidx = 0;
        while (cline[lidx] && iswhite(cline[lidx])) lidx++;

        // Look for a prefix
        if (cline[lidx] == '[') {
            lidx++;
            eidx = lidx;
            while (cline[eidx] && cline[eidx] != ']') eidx++;
            if (eidx == lidx) {
                // No prefix
                prefix = NULL;
            } else {
                prefix = CORD_cat(CORD_substr(cline, lidx, eidx - lidx), ".");
            }
            lidx = eidx;
            continue;
        }

        // Parse the condition, if present
        CORD condition = NULL;
        if (cline[lidx] == '/') {
            // There is a condition, find its end
            lidx++;
            eidx = lidx;
            while (cline[eidx] && cline[eidx] != '/') {
                if (cline[eidx] == '\\' && cline[eidx+1] == '/') eidx++;
                eidx++;
            }
            condition = CORD_substr(cline, lidx, eidx - lidx);
            lidx = eidx;
            if (cline[lidx] == '/') lidx++;
        }

        // Parse the directive name, which had better be present!
        CORD directive = NULL;
        while (cline[lidx] && iswhite(cline[lidx])) lidx++;
        eidx = lidx;
        while (cline[eidx] && isid(cline[eidx])) eidx++;
        if (eidx == lidx) continue;
        directive = CORD_cat(prefix, CORD_substr(line, lidx, eidx - lidx));
        lidx = eidx;

        // We need either an = or a += here
        bool extension = false;
        bool left = false;
        while (cline[lidx] && iswhite(cline[lidx])) lidx++;
        if (cline[lidx] == '=') {
            lidx++;
        } else if (cline[lidx] == '+' && cline[lidx+1] == '=') {
            extension = true;
            lidx += 2;
        } else if (cline[lidx] == '<' && cline[lidx+1] == '+' && cline[lidx+2] == '=') {
            extension = true;
            left = true;
            lidx += 3;
        } else {
            // Invalid!
            continue;
        }

        // Now the actual value
        CORD value = NULL;
        while (cline[lidx] && iswhite(cline[lidx])) lidx++;
        size_t ci;
        for (ci = lidx; cline[ci]; ci++) {
            if (cline[ci] == '\\' && cline[ci+1]) {
                // Add what we have so far
                if (ci > lidx)
                    value = CORD_cat(value, CORD_substr(cline, lidx, ci - lidx));

                // Handle the special character
                char add = cline[ci+1];
                switch (cline[ci+1]) {
                    case 'n': add = '\n'; break;
                    case 'r': add = '\r'; break;
                    case 't': add = '\t'; break;
                    case 'v': add = '\v'; break;
                }
                if (add == '$') {
                    /* We turn this into $$ so that the variable processor
                     * doesn't expand it */
                    value = CORD_cat_char(value, '$');
                }
                value = CORD_cat_char(value, add);

                ci++;
                lidx = ci + 1;
            }
        }
        if (ci > lidx)
            value = CORD_cat(value, CORD_substr(cline, lidx, ci - lidx));

        // Put it together
        Directive *nd = GC_NEW(Directive);
        nd->condition = condition;
        nd->extension = extension;
        nd->left = left;
        nd->value = value;

        if (csc_verbose)
            fprintf(stderr, "^CONFIG: Directive %s%s%s%s%s%s, value %s\n",
                CORD_to_char_star(directive),
                condition?", condition ":"", condition?CORD_to_char_star(condition):"",
                extension?", ":"", (extension&&left)?"left ":"", extension?"extension":"",
                CORD_to_char_star(value));

        // If it's not an extension and it's unconditional, we can replace it regardless
        if (extension || condition) {
            // Check if the directive is already there
            Directive *entry = csc_htGet(config, directive);
            if (entry) {
                // OK, extend it
                while (entry->next) entry = entry->next;
                entry->next = nd;
                continue;
            }
        }

        // It's not there, so replace it outright
        csc_htAdd(config, directive, nd);
    }
}

// Load main config
void csc_configInit(const char *configFile)
{
    // Load our configuration
    if (configFile) {
        csc_configTree = csc_newHashTable();
        csc_extendConfig(csc_configTree, NULL);
        csc_extendConfig(csc_configTree, configFile);
    } else {
        csc_configTree = csc_loadConfig(".", "plip.ini", true);
    }
}

// Load configuration files with the given name starting from the given directory
CSC_Config *csc_loadConfig(CORD dir, CORD file, bool parents)
{
    CSC_Config *ret = csc_newHashTable();

    for (size_t ri = parents ? 0 : (MAX_CONFIG_DEPTH-1); ri < MAX_CONFIG_DEPTH; ri++) {
        if (ri == 0) {
            // Start with the default config
            csc_extendConfig(ret, NULL);
        } else {
            // Create our parent links
            CORD cur = dir;
            for (size_t rpi = ri+1; rpi < MAX_CONFIG_DEPTH; rpi++)
                cur = CORD_cat(cur, CORD_cat(CSC_DIRSEP, ".."));

            // Add this directory
            cur = CORD_cat(cur, CORD_cat(CSC_DIRSEP, file));
            csc_extendConfig(ret, cur);
        }

    }

    return ret;
}

/* Read a configuration directive with the given condition and the provided
 * (potentially NULL) variable definitions */
CORD csc_configRead(CSC_Config *config, CORD directive, CORD condition, CSC_HashTable *variables)
{
    // Do we have an entry for it?
    Directive *d = csc_htGet(config, directive);
    if (!d) return NULL;

    // Put together the prevar string
    CORD prevar = NULL;
    for (; d; d = d->next) {
        // Do we match this condition?
        if (d->condition) {
            // Check whether the condition applies
            if (!csc_match(d->condition, condition))
                continue;
        }

        // Add it on
        if (d->extension) {
            if (d->left)
                prevar = CORD_cat(d->value, prevar);
            else
                prevar = CORD_cat(prevar, d->value);
        } else {
            prevar = d->value;
        }
    }
    char *cprevar = CORD_to_char_star(prevar);

    // If it's blank, we're done!
    if (!cprevar[0]) return NULL;

    // Handle vars
    CORD ret = NULL;
    size_t start = 0, pi;
    for (pi = 0; cprevar[pi]; pi++) {
        if (cprevar[pi] == '$') {
            // Add what we have so far
            if (pi > start)
                ret = CORD_cat(ret, CORD_substr(cprevar, start, pi - start));

            // Possibly a variable reference
            if (cprevar[pi+1] == '$') {
                // Nope!
                pi++;
                start = pi;
                continue;
            }

            CORD variable = NULL;

            // Get out the variable name
            if (cprevar[pi+1] == '(') {
                // Bracketed
                size_t varStart = pi+2;
                size_t varEnd;
                for (varEnd = varStart; cprevar[varEnd] && cprevar[varEnd] != ')'; varEnd++);
                variable = CORD_substr(cprevar, varStart, varEnd - varStart);
                if (cprevar[varEnd]) varEnd++;
                pi = varEnd - 1;

            } else {
                // Unbracketed
                size_t varStart = pi+1;
                size_t varEnd;
                for (varEnd = varStart; cprevar[varEnd] && isid(cprevar[varEnd]); varEnd++);
                variable = CORD_substr(cprevar, varStart, varEnd - varStart);
                if (cprevar[varEnd]) varEnd++;
                pi = varEnd - 1;

            }
            start = pi + 1;

            // Get out the variable value
            CORD varVal = variables ? csc_htGet(variables, variable) : NULL;
            ret = CORD_cat(ret, varVal);
        }
    }
    if (pi > start)
        ret = CORD_cat(ret, CORD_substr(cprevar, start, pi - start));

    return ret;
}

/* Reader for booleans */
bool csc_configBool(CSC_Config *config, CORD directive, CORD condition)
{
    CORD raw = csc_configRead(config, directive, condition, NULL);
    char c = CORD_fetch(raw, 0);
    return !(c == '0' ||
             c == 'F' || c == 'f' ||
             c == 'N' || c == 'n' ||
             c == '\0');
}

/* Reader for ints */
int csc_configInt(CSC_Config *config, CORD directive, CORD condition)
{
    char *raw = CORD_to_char_star(csc_configRead(config, directive, condition, NULL));
    return atoi(raw);
}

/* Reader for doubles */
double csc_configDouble(CSC_Config *config, CORD directive, CORD condition)
{
    char *raw = CORD_to_char_star(csc_configRead(config, directive, condition, NULL));
    return atof(raw);
}

/* Simple read from default config */
CORD csc_config(CORD directive)
{
    return csc_configRead(csc_configTree, directive, NULL, NULL);
}
