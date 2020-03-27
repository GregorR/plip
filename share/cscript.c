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

#define _POSIX_C_SOURCE 200112L // for fdopen, *env

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <glob.h>
#include <sys/wait.h>
#endif

#ifndef NO_PCRE
#include <pcre.h>
#endif

#include "cscript.h"

bool csc_verbose = false;

#define VERBOSE(args...) do { \
    if (csc_verbose) \
        CORD_fprintf(stderr, args); \
} while(0)

#ifdef _WIN32
// Windows is trash; this quotes its arguments nicely.
CORD csc_windowsArgs(char *const argv[])
{
    CORD ret = CORD_EMPTY;
    static const char special[] = " \n\t\v\"";

    for (int ai = 0; argv[ai]; ai++) {
        const char *arg = argv[ai];
        size_t arglen = strlen(arg);

        // Add the separator if necessary
        if (ai > 0)
            ret = CORD_cat(ret, " ");

        // If it's zero-length, put quotes to make it real
        if (arglen == 0) {
            ret = CORD_cat(ret, "\"\"");
            continue;
        }

        // If it has no special characters, just add it
        if (strcspn(arg, special) == arglen) {
            ret = CORD_cat(ret, arg);
            continue;
        }

        // Special characters, so quote it
        ret = CORD_cat(ret, "\"");
        for (size_t si = 0; si < arglen; si++) {
            size_t numBackslashes = 0;
            for (; arg[si] == '\\'; si++) numBackslashes++;

            if (si == arglen || arg[si] == '"') {
                // Backslashes are special before " and before the end
                while (numBackslashes--)
                    ret = CORD_cat(ret, "\\\\");

                if (arg[si] == '"')
                    ret = CORD_cat(ret, "\\\"");

            } else {
                // Unremarkable backslashes
                while (numBackslashes--)
                    ret = CORD_cat(ret, "\\");
                ret = CORD_cat_char(ret, arg[si]);

            }
        }
        ret = CORD_cat(ret, "\"");
    }

    VERBOSE("Processed command: %r\n", ret);

    return ret;
}
#endif

// cscript initialization tasks
void csc_init(const char *arg0)
{
    GC_INIT();

    // Make sure we're in PATH
    if (strchr(arg0, CSC_DIRSEP[0])) {
        // We were called as a path, so add it to PATH
        CORD dir = csc_absolute(csc_dirname(arg0));
        CORD path = getenv("PATH");
        if (path) {
#ifdef _WIN32
            path = csc_casprintf("PATH=%r;%r", dir, path);
            putenv(CORD_to_char_star(path));
#else
            path = csc_casprintf("%r:%r", dir, path);
            setenv("PATH", CORD_to_char_star(path), 1);
#endif
        }
    }
}

// Like CORD_sprintf, but just return the cord directly
CORD csc_casprintf(CORD format, ...)
{
    va_list args;
    va_start(args, format);
    return csc_cvasprintf(format, args);
}

CORD csc_cvasprintf(CORD format, va_list args)
{
    CORD ret;
    CORD_vsprintf(&ret, format, args);
    return ret;
}

// Similar, but drop to a regular string
char *csc_asprintf(CORD format, ...)
{
    va_list args;
    va_start(args, format);
    return csc_vasprintf(format, args);
}

char *csc_vasprintf(CORD format, va_list args)
{
    return CORD_to_char_star(csc_cvasprintf(format, args));
}

// Does this file exist?
bool csc_fileExists(CORD filename)
{
    char *cFilename = CORD_to_char_star(filename);
    bool exists = false;
    if (!access(cFilename, F_OK))
        exists = true;

    VERBOSE("%r exists? %c\n", filename, exists?'T':'F');
    return exists;
}

// Portable globbing, just returns a NULL-terminated CORD array
CORD *csc_glob(CORD pattern)
{
#ifdef _WIN32
    intptr_t fh;
    const char *cpattern = CORD_to_char_star(pattern);
    struct _finddata_t fd;
    size_t retSz = 1, retLen = 0;
    CORD *ret = GC_MALLOC(sizeof(CORD));
    if ((fh = _findfirst(cpattern, &fd)) != -1) {
        while (1) {
            if (retLen >= retSz) {
                retSz *= 2;
                ret = GC_REALLOC(ret, retSz * sizeof(CORD));
            }
            ret[retLen++] = GC_STRDUP(fd.name);

            if (_findnext(fh, &fd) != 0)
                break;
        }
        _findclose(fh);
    }

    if (retLen >= retSz) {
        retSz++;
        ret = GC_REALLOC(ret, retSz * sizeof(CORD));
    }
    ret[retLen++] = NULL;

#else // !_WIN32
    glob_t pglob;
    int gret = glob(CORD_to_char_star(pattern), 0, NULL, &pglob);
    if (gret < 0) // FIXME?
        return NULL;

    // Turn them into a cord array
    CORD *ret = GC_MALLOC((pglob.gl_pathc + 1) * sizeof(CORD));
    size_t ri;
    for (ri = 0; ri < pglob.gl_pathc; ri++)
        ret[ri] = GC_STRDUP(pglob.gl_pathv[ri]);
    ret[ri] = NULL;

    globfree(&pglob);

#endif

    if (csc_verbose) {
        VERBOSE("glob %r (", pattern);
        for (size_t ri = 0; ret[ri]; ri++)
            VERBOSE("%r, ", ret[ri]);
        VERBOSE("-)\n");
    }

    return ret;
}

// Absolute-ize a path
CORD csc_absolute(CORD path)
{
#ifdef _WIN32
    char *cpath = CORD_to_char_star(path);
    char outBuf[MAX_PATH];
    if (GetFullPathName(cpath, MAX_PATH, outBuf, NULL) > 0)
        path = GC_STRDUP(outBuf);

#else
    bool isAbsolute = false;
    isAbsolute = (CORD_len(path) > 0 && CORD_fetch(path, 0) == '/');

    if (!isAbsolute) {
        // Prepend the cwd
#define WDBUFSZ 4096
        char *cwdBuf = GC_MALLOC_ATOMIC(WDBUFSZ);
        if (getcwd(cwdBuf, WDBUFSZ)) {
            path = CORD_cat(
                cwdBuf, CORD_cat(CSC_DIRSEP, path));
        }
#undef WDBUFSZ
    }

#endif

    VERBOSE("absolute -> %r\n", path);

    return path;
}

// Get the dirname (INCLUDING terminating /) from a path
CORD csc_dirname(CORD path)
{
    size_t endSlash = CORD_rchr(path, CORD_len(path)-1, CSC_DIRSEP[0]);
    if (endSlash != (size_t) -1) {
        // Cut it out
        path = CORD_substr(path, 0, endSlash + 1);
    }
    return path;
}

// Portable link
void csc_ln(CORD from, CORD to)
{
    char *cfrom = CORD_to_char_star(from);
    char *cto = CORD_to_char_star(to);

    VERBOSE("ln(%r, %r)\n", from, to);

#ifdef _WIN32
    // Try hard linking
    if (!CreateHardLink(cto, cfrom, NULL)) {
        // Alright, try copying
        if (!CopyFile(cfrom, cto, TRUE))
            CRASH(cto);
    }

#else
    if (link(cfrom, cto) < 0)
        CRASH(cto);

#endif
}

// Read an entire file
CORD csc_readFile(CORD filename)
{
    VERBOSE("read(%r)\n", filename);

    char *cFilename = CORD_to_char_star(filename);
    FILE *fh = fopen(cFilename, "rb");
    if (!fh)
        return NULL;
    CORD ret = CORD_from_file(fh);
    return ret;
}

// Write an entire file
bool csc_writeFile(CORD filename, CORD content)
{
    VERBOSE("write(%r, %d)\n", filename, CORD_len(content));

    char *cFilename = CORD_to_char_star(filename);
    FILE *fh = fopen(cFilename, "wb");
    if (!fh)
        return false;
    if (CORD_put(content, fh) != 1) {
        fclose(fh);
        return false;
    }

    fclose(fh);
    return true;
}

/* Run a program and capture its output into a cord. Returns the exit code of
 * the program, or -1 for errors. Set STDIN in fds to NOT redirect input from
 * /dev/null. Set STDOUT or STDERR in fds to capture them. */
int csc_run(int fds, CORD *into, char *const argv[])
{
    if (csc_verbose) {
        VERBOSE("run");
        for (size_t ai = 0; argv[ai]; ai++)
            VERBOSE(" %s", argv[ai]);
        VERBOSE("\n");
    }

#ifdef _WIN32
    char *cmd = CORD_to_char_star(csc_windowsArgs(argv));
    PROCESS_INFORMATION pi = { 0 };
    STARTUPINFO si = { 0 };
    BOOL success = FALSE;
    SECURITY_ATTRIBUTES sa = { 0 };
    HANDLE cstdin[2], cstdout[2];

    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;

    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    // Handle stdin
    if (!(fds & CSC_STDIN)) {
        // Redirect stdin from /dev/null(ish)
        CreatePipe(&cstdin[0], &cstdin[1], &sa, 0);
        CloseHandle(cstdin[1]);
        si.hStdInput = cstdin[0];
    } else {
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    }

    // Handle the out/err pipe
    if (fds & CSC_STDOUTERR)
        CreatePipe(&cstdout[0], &cstdout[1], &sa, 0);

    // And assign it
    if (fds & CSC_STDOUT)
        si.hStdOutput = cstdout[1];
    else
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    if (fds & CSC_STDERR)
        si.hStdError = cstdout[1];
    else
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    // Create the child process
    success = CreateProcess(
        NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);

    // Close irrelevant pipes
    if (!(fds & CSC_STDIN))
        CloseHandle(cstdin[0]);
    if (fds & CSC_STDOUTERR)
        CloseHandle(cstdout[1]);
    CloseHandle(pi.hThread);

    if (!success)
        return -1;

    // Read its output if asked
    if (fds & CSC_STDOUTERR) {
        int outfd = _open_osfhandle((intptr_t) cstdout[0], _O_RDONLY);
        FILE *outfh = _fdopen(outfd, "rb");
        *into = CORD_from_file_eager(outfh);
        CloseHandle(pi.hProcess);

    } else {
        // Otherwise, wait for it
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        if (into)
            *into = CORD_EMPTY;

    }

    return 0; // FIXME

#else
    // Presumably Unix
    int cstdout[2];
    pid_t cpid;

    if (fds & CSC_STDOUTERR) {
        // Redirecting output, make a pipe
        if (pipe(cstdout) < 0) CRASH("pipe");
    }

    // Fork the child
    cpid = fork();
    if (cpid < 0) CRASH("fork");
    if (cpid == 0) {
        // Use the stdout pipe
        if (fds & CSC_STDOUT)
            dup2(cstdout[1], 1);
        if (fds & CSC_STDERR)
            dup2(cstdout[1], 2);
        if (fds & CSC_STDOUTERR) {
            close(cstdout[0]);
            close(cstdout[1]);
        }

        // Redirect stdin
        if (!(fds & CSC_STDIN)) {
            int sinfd = open("/dev/null", O_RDONLY);
            if (sinfd < 0) CRASH("/dev/null");
            dup2(sinfd, 0);
            close(sinfd);
        }

        // And run the child
        execvp(argv[0], argv);
        exit(1);
    }

    // Read its output
    if (fds & CSC_STDOUTERR) {
        close(cstdout[1]);
        FILE *fh = fdopen(cstdout[0], "rb");
        *into = CORD_from_file_eager(fh);

    }

    // Wait for it
    int ret;
    waitpid(cpid, &ret, 0);

    if (WIFEXITED(ret))
        return WEXITSTATUS(ret);
    return -1;

#endif
}

// Wrapper for csc_run to take the arguments directly
int csc_runl(int fds, CORD *into, const char *arg, ...)
{
    va_list args;
    char *narg;
    int argc = 1;

    // First just count
    va_start(args, arg);
    while (1) {
        narg = va_arg(args, char *);
        argc++;
        if (!narg)
            break;
    }
    va_end(args);

    char *argv[argc];

    // Then copy
    argv[0] = (char *) arg;
    va_start(args, arg);
    for (int argi = 1; argi < argc; argi++) {
        narg = va_arg(args, char *);
        argv[argi] = narg ? CORD_to_char_star(narg) : NULL;
    }

    return csc_run(fds, into, argv);
}

/* Run a program with its input and output piped. Provide an fd greater than -1
 * as stdinFd to pipe input, an output fd is returned or -1 for error. If
 * stdinFd is provided, it is closed. */
int csc_runp(int stdinFd, char *const argv[])
{
    if (csc_verbose) {
        VERBOSE("pipe");
        for (size_t ai = 0; argv[ai]; ai++)
            VERBOSE(" %s", argv[ai]);
        VERBOSE("\n");
    }

#ifdef _WIN32
    char *cmd = CORD_to_char_star(csc_windowsArgs(argv));
    PROCESS_INFORMATION pi = { 0 };
    STARTUPINFO si = { 0 };
    BOOL success = FALSE;
    SECURITY_ATTRIBUTES sa = { 0 };
    HANDLE cstdin, cstdout[2];

    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;

    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    // Handle stdin
    if (stdinFd >= 0) {
        cstdin = (HANDLE) _get_osfhandle(stdinFd);

    } else {
        // Make a dead pipe for it
        HANDLE cstdinW;
        CreatePipe(&cstdin, &cstdinW, &sa, 0);
        CloseHandle(cstdinW);

    }
    si.hStdInput = cstdin;

    // Handle the output pipe
    CreatePipe(&cstdout[0], &cstdout[1], &sa, 0);
    si.hStdOutput = cstdout[1];

    // And make sure error stays as it is
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    // Create the child process
    success = CreateProcess(
        NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);

    // Close irrelevant pipes
    if (stdinFd >= 0)
        _close(stdinFd);
    else
        CloseHandle(cstdin);
    CloseHandle(cstdout[1]);

    if (!success) {
        CloseHandle(cstdout[0]);
        return -1;
    }

    // Now prepare output as an fd
    return _open_osfhandle((intptr_t) cstdout[0], _O_RDONLY);

#else
    // Presumably Unix
    int cstdout[2];
    pid_t cpid;

    // Prepare the output pipe
    if (pipe(cstdout) < 0) CRASH("pipe");

    // Fork the child
    cpid = fork();
    if (cpid < 0) CRASH("fork");
    if (cpid == 0) {
        // Use the stdout pipe
        dup2(cstdout[1], 1);

        // Redirect stdin
        if (stdinFd >= 0) {
            dup2(stdinFd, 0);
        } else {
            int sinfd = open("/dev/null", O_RDONLY);
            if (sinfd < 0) CRASH("/dev/null");
            dup2(sinfd, 0);
            close(sinfd);
        }

        // And run the child
        execvp(argv[0], argv);
        exit(1);
    }

    // Close irrelevant pipes
    if (stdinFd >= 0)
        close(stdinFd);
    close(cstdout[1]);

    return cstdout[0];

#endif
}

// runp with arguments directly
int csc_runpl(int stdinFd, const char *arg, ...)
{
    va_list args;
    char *narg;
    int argc = 1;

    // First just count
    va_start(args, arg);
    while (1) {
        narg = va_arg(args, char *);
        argc++;
        if (!narg)
            break;
    }
    va_end(args);

    char *argv[argc];

    // Then copy
    argv[0] = (char *) arg;
    va_start(args, arg);
    for (int argi = 1; argi < argc; argi++) {
        narg = va_arg(args, char *);
        argv[argi] = narg ? CORD_to_char_star(narg) : NULL;
    }

    return csc_runp(stdinFd, argv);
}

// Wait for a pipeline by way of waiting for a pipe
bool csc_wait(int fd)
{
#define BUFSZ 4096
    char *buf = GC_MALLOC_ATOMIC(BUFSZ);
    ssize_t rd;
    while ((rd = read(fd, buf, BUFSZ)) > 0);
    close(fd);
    if (rd < 0)
        return false;
    return true;
#undef BUFSZ
}

#ifndef NO_PCRE
// Match a string against a regex. Returns all matches as an array.
CORD *csc_match(CORD pattern, CORD input)
{
    VERBOSE("match /%r/ %r", pattern, input);

    // Prepare the PCRE
    const char *errptr;
    int erroffset;
    char *cpattern = CORD_to_char_star(pattern);
    pcre *re = pcre_compile(cpattern, 0, &errptr, &erroffset, NULL);
    if (!re) CRASH(cpattern);

    int captureCt;
    if (pcre_fullinfo(re, NULL, PCRE_INFO_CAPTURECOUNT, &captureCt) < 0)
        CRASH(cpattern);

    // Match it
    int ovecsize = (captureCt+1)*3;
    int ovector[ovecsize];
    int pret = pcre_exec(re, NULL, CORD_to_char_star(input), CORD_len(input), 0,
        0, ovector, ovecsize);
    pcre_free(re);
    if (pret == PCRE_ERROR_NOMATCH) {
        VERBOSE(": no match\n");
        return NULL;
    }
    if (pret < 0)
        CRASH(cpattern);

    VERBOSE(": match (%d)\n", (int) captureCt);

    // Extract the bits
    CORD *ret = GC_MALLOC((captureCt+1)*sizeof(CORD));
    for (int i = 0; i <= captureCt; i++) {
        int start = ovector[i*2];
        int end = ovector[i*2+1];
        ret[i] = CORD_substr(input, start, end - start);
    }

    return ret;
}
#endif

// Split input by line
CORD *csc_lines(CORD input)
{
    size_t lineStart = 0, inputEnd = CORD_len(input);
    size_t retSz = 1, retLen = 0;
    CORD *ret = GC_MALLOC(retSz * sizeof(CORD));
    while (lineStart < inputEnd) {
        size_t lineEnd = CORD_chr(input, lineStart, '\n');
#ifdef _WIN32
        // Cope with broken line endings
        size_t lineEndPre = CORD_chr(input, lineStart, '\r');
        if (lineEndPre < lineEnd - 1 || lineEndPre > lineEnd)
            lineEndPre = lineEnd;
#else
#define lineEndPre lineEnd
#endif
        CORD line;
        if (lineEnd < inputEnd) {
            line = CORD_substr(input, lineStart, lineEndPre - lineStart);
            lineStart = lineEnd + 1;
        } else {
            line = CORD_substr(input, lineStart, inputEnd - lineStart);
            lineStart = lineEnd;
        }
#ifndef _WIN32
#undef lineEndPre
#endif

        // Discard blank lines as NULL is our sentinel
        if (line == NULL)
            continue;

        // Add it to our list
        if (retLen >= retSz) {
            retSz *= 2;
            ret = GC_REALLOC(ret, retSz * sizeof(CORD));
        }
        ret[retLen++] = line;
    }

    // Leave a NULL breadcrumb
    if (retLen >= retSz) {
        retSz++;
        ret = GC_REALLOC(ret, retSz * sizeof(CORD));
    }
    ret[retLen] = NULL;

    return ret;
}

#ifndef NO_PCRE
// Select lines matching the given PCRE regex
CORD *csc_grep(CORD pattern, CORD input)
{
    VERBOSE("grep /%r/ (%d)\n", pattern, CORD_len(input));

    // Prepare the PCRE
    const char *errptr;
    int erroffset;
    char *cpattern = CORD_to_char_star(pattern);
    pcre *re = pcre_compile(cpattern, 0, &errptr, &erroffset, NULL);
    if (!re) CRASH(cpattern);

    int ovecsize = 33;
    int ovector[ovecsize];

    // FIXME: Maybe pcre_study

    // Split it into lines
    CORD *lines = csc_lines(input);

    // Go line by line
    size_t retSz = 1, retLen = 0;
    CORD *ret = GC_MALLOC(retSz * sizeof(CORD));
    for (size_t li = 0; lines[li]; li++) {
        CORD line = lines[li];

        // Test if it matches
        int pret = pcre_exec(re, NULL, CORD_to_char_star(line), CORD_len(line),
            0, 0, ovector, ovecsize);
        if (pret == PCRE_ERROR_NOMATCH)
            continue;
        if (pret < 0)
            CRASH(cpattern);

        // Add it to our list
        if (retLen >= retSz) {
            retSz *= 2;
            ret = GC_REALLOC(ret, retSz * sizeof(CORD));
        }
        ret[retLen++] = line;
    }

    // Leave a NULL breadcrumb
    if (retLen >= retSz) {
        retSz++;
        ret = GC_REALLOC(ret, retSz * sizeof(CORD));
    }
    ret[retLen] = NULL;

    pcre_free(re);

    return ret;
}
#endif
