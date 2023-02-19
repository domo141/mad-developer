#if 0 /* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*-
 set -euf;
 case ${1-} in *''/*) trg=$1; shift ;; *) trg=${0##*''/}; trg=${trg%.c}
 esac; test -e "$trg" && rm "$trg"
 case ${1-} in '') set x -O2; shift; esac
 x_exec () { printf %s\\n "$*" >&2; exec "$@"; }
 x_exec ${CC:-gcc} -std=c11 "$@" -o "$trg" "$0"
 exit $?
 */
#endif
/*
 * $ exitsshconns.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *      Copyright (c) 2023 Tomi Ollila
 *          All rights reserved
 *
 * Created: Sun 12 Feb 2023 20:36:17 EET too
 * Last modified: Sun 19 Feb 2023 12:24:13 +0200 too
 */

// SPDX-License-Identifier: BSD 2-Clause "Simplified" License

#include "more-warnings.h"

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdalign.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

// (variable) block begin/end -- explicit liveness...
#define BB {
#define BE }

#define isizeof (int)sizeof

static int timestamp(char * buf)
{
    tzset();
    time_t t = time(NULL) - timezone;
    return snprintf(buf, 16, "%02ld:%02ld:%02ld: ", // max len 13 '-23:-59:...'
                    (t % 86400) / 3600, (t % 3600) / 60, t % 60);
}

static void __attribute__ ((format (printf, 1, 2))) __attribute__ ((noreturn))
    die(const char * fmt, ...)
{
    int e = errno;
    char buf[512];
    int l = timestamp(buf);

    va_list ap;
    va_start(ap, fmt);
    l += vsnprintf(buf + l, sizeof buf - l, fmt, ap);
    va_end(ap);
    if (l >= isizeof buf || buf[l - 1] != ':') {
        if (l >= isizeof buf) l = isizeof buf - 1;
        buf[l] = '\n';
        (void)!write(2, buf, l + 1);
        exit(1);
    }
    char * p = buf + l;
    int len = isizeof buf - l;
    l = snprintf(p, len, " %s", strerror(e));
    if (l >= len) l = len - 1;
    p[l] = '\n';
    (void)!write(2, buf, p - buf + l + 1);
    exit(1);
}

static void __attribute__ ((format (printf, 1, 2)))
    msg(const char * fmt, ...)
{
    char buf[512];
    int l = timestamp(buf);

    va_list ap;
    va_start(ap, fmt);
    l += vsnprintf(buf + l, sizeof buf - l, fmt, ap);
    va_end(ap);
    if (l >= isizeof buf) l = isizeof buf - 1;
    buf[l] = '\n';
    (void)!write(2, buf, l + 1);
}

// hint: gcc -dM -E -xc /dev/null
// and: clang -dM -E -xc /dev/null

#if ! defined (__GNUC__) || defined (__clang__)
static int read_more(int fd, char * buf, int bufsiz, char ** p, int *l)
{
    *p = buf;
    *l = read(fd, buf, bufsiz);
    //*l = read(fd, buf, 27); // test line
    if (*l <= 0) return -1;
    return ((unsigned char *)buf)[0];
}
#endif

/* sock_diag(7) approach may be better than this, wrote this after tldr
 * moment after reading a few first lines of ss(1) source code) ;D
 */
static int read_procnettcp(const char * fnam, char * obuf, int bufsiz)
{
    int fd = open(fnam, O_RDONLY);
    if (fd < 0) return 0;

    char buf[4096];
    char * p;
    int l = 0;

#if defined (__GNUC__) && ! defined (__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic" // ISO C forbids nested functions
    /* nested function is GNU C extension */
    int read_more(void) {
        p = buf;
        l = read(fd, buf, bufsiz);
        if (l <= 0) return -1;
        return ((unsigned char *)buf)[0];
    }
#pragma GCC diagnostic pop
    int c = read_more();
#define next_byte() (l-- > 0) ? *p++ : read_more()
#else
    int c = read_more(fd, buf, sizeof buf, &p, &l);
#define next_byte() (l-- > 0) ? *p++ : read_more(fd, buf, sizeof buf, &p, &l)
#endif

    /* note that in most cases below it is unlikely EOF was reached */
    /* skip header line */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
    while (c >= 0 && c != '\n') c = next_byte(); c = next_byte();
    char * op = obuf;
    do {
        // pass thru '^  n:'
        while (c >= 0 && c != ':') c = next_byte(); c = next_byte();
        // second ':'
        while (c >= 0 && c != ':') c = next_byte(); c = next_byte();
        // expect port 22 (0016)
        if (c != '0') goto _weol; c = next_byte();
        if (c != '0') goto _weol; c = next_byte();
        if (c != '1') goto _weol; c = next_byte();
        if (c != '6') goto _weol; c = next_byte();
        if (c != ' ') goto _weol; c = next_byte();
        // pass thru rem_address
        while (c >= 0 && c != ' ') c = next_byte(); c = next_byte();
        // well, c should always be 0, as range is 1 - 11 (0x01 - 0x0b)
        // TCP_ESTABLISHED - TCP_CLOSING in netinet/tcp.h
        if (c != '0') goto _weol; c = next_byte();
        if (c == 'A') goto _weol; c = next_byte(); // skip listening socket
        // skip 2 more :s
        while (c >= 0 && c != ':') c = next_byte(); c = next_byte();
        while (c >= 0 && c != ':') c = next_byte(); c = next_byte();
        // then some spaces // ' 'retrnsmt' ' uid
        while (c >= 0 && c != ' ') c = next_byte(); c = next_byte();
        while (c >= 0 && c != ' ') c = next_byte(); c = next_byte();
        while (c == ' ') c = next_byte(); c = next_byte();
        // skip uid to next space
        while (c >= 0 && c != ' ') c = next_byte(); c = next_byte();
        while (c == ' ') c = next_byte(); c = next_byte();
        // now at timeout, skip to next space (one before inode)
        while (c >= 0 && c != ' ') c = next_byte(); c = next_byte();
        // (only?) uid and timeout has leading spaces -- just now ... :O //
        if (c == '0') goto _weol;
        // so, now at inode (which is not zero). copy it
        bufsiz--;
        do {
            if (bufsiz == 0) goto _ret;
            *op++ = c;
            bufsiz--;
            c = next_byte();
        } while (c >= 0 && c != ' ');
        *op++ = '/';
        c = next_byte();
    _weol:
        while (c >= 0 && c != '\n') c = next_byte(); c = next_byte();
#pragma GCC diagnostic pop
#undef next_byte
    } while (c >= 0); // usually c < 0 here when c < 0 //
 _ret:
    close(fd);
    return op - obuf;
}

static int xcmp(const char * s1, const char *s2)
{
    const char *s1a = s1 + 1;
    while (*s1++ == *s2 && *s2++ != '/') continue;
    return s1 - s1a;
}

int main(void)
{
    alignas(4) char inodez[32768];  // but did not help for the cast-align...
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align" // ...armv7hl, but not aarch64 :O
    ((uint32_t *)inodez)[8191] = 0x23232323; // "####", just for extra sure //
#pragma GCC diagnostic pop
    BB;
    int l = read_procnettcp("/proc/net/tcp", inodez, sizeof inodez - 4);
    if (l < isizeof inodez - 4096) {
        l += read_procnettcp("/proc/net/tcp6",
                             inodez + l, sizeof inodez - l - 4);
    }
    if (l == 0) {
        msg("no inbound ssh connections to exit");
        return 0;
    }
    inodez[l++] = '#';
    BE;
    int kc = 0;
#if 1
    DIR * dir1 = opendir("/proc");
    if (dir1 == NULL) die("Cannot open '/proc':");
    for (struct dirent * de1; (de1 = readdir(dir1)) != NULL;) {
        if (de1->d_name[0] < '0' || de1->d_name[0] > '9') continue;
        int dl = strlen(de1->d_name);
        if (dl > 32) continue; // too lazy to add msg -- maeby later...
        char pidfdd[64];
        memcpy(pidfdd, de1->d_name, dl);
        strcpy(pidfdd + dl, "/fd");
        int fd = openat(dirfd(dir1), pidfdd, O_RDONLY);
        if (fd < 0) {
            //msg("Cannot open /proc/%s:", pidfdd);
            continue;
        }
        DIR * dir2 = fdopendir(fd);
        if (dir2 == NULL) die("Cannot fopendir(%d):", fd); // unlikely
        for (struct dirent * de2; (de2 = readdir(dir2)) != NULL;) {
            //printf("de2: %s - %s\n", pidfdd, de2->d_name);
            if (de2->d_name[0] < '0' || de2->d_name[0] > '9') continue;
            char linkname[64];
            int ll = readlinkat(dirfd(dir2), de2->d_name,
                                linkname, sizeof linkname);
            if (ll < 0) {
                msg("Cannot readlink /proc/%s/%s:", pidfdd, de2->d_name);
                continue;
            }
            //if (ll >= isizeof linkname) continue; // socket links shorter...
            if (linkname[0] != 's') continue; // not socket:[...]
            if (memcmp(linkname, "socket:[", 8) != 0) continue; // ditto
            /* find matching socket inodes */
            for (const char * s2 = inodez; *s2 != '#'; s2++) {
                int sl = xcmp(linkname + 8, s2);
                if (s2[sl] == '/' && linkname[sl + 8] == ']') {
                    pid_t pid = atoi(pidfdd); kill(pid, 9);
                    //linkname[ll] = '\0'; printf("%d %s\n", pid, linkname);
                    kc++;
                    goto pidfound; // "break" 2 for loops
                }
                /* else */
                s2 += sl;
                while (*s2 != '/') s2++;
            }
        }
    pidfound:
        closedir(dir2);
    }
    closedir(dir1);
#endif
    //inodez[l++] = '\n'; (void)!write(1, inodez, l);
    msg("%d processes exited: (~%d conns)", kc, kc/2);
    return 0;
}
