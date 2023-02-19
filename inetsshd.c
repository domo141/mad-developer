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
 * $ inetsshd.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *      Copyright (c) 2023 Tomi Ollila
 *          All rights reserved
 *
 * Created: Sat 14 Jan 2023 14:30:53 EET too
 * Last modified: Mon 30 Jan 2023 23:01:56 +0200 too
 */
// SPDX-License-Identifier: BSD 2-Clause "Simplified" License

#include "more-warnings.h"

//#define _GNU_SOURCE // if this is needed, remove extern char ** environ later

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

// shared library and python ctypes would be interesting to try,
// but little gain, and harder to test, so...

#define null ((void*)0)
#define isizeof (int)sizeof

// hint: gcc -dM -E -xc /dev/null | grep -i BYTE_ORDER

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

#define IADDR(a,b,c,d) ((in_addr_t)((a << 24) + (b << 16) + (c << 8) + d))
#define IPORT(v) ((in_port_t)(v))

#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

#define IADDR(a,b,c,d) ((in_addr_t)(a + (b << 8) + (c << 16) + (d << 24)))
#define IPORT(v) ((in_port_t)(((v) >> 8) | ((v) << 8)))
#define A256(a) (a)

#else
#error unknown BYTE_ORDER
#endif

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

static void xdup2(int oldfd, int newfd)
{
    if (dup2(oldfd, newfd) < 0) die("dup2()!");
}

//not needed after all...
//static void close_3(void) { write(1, "XXXX\n", 5); close(3); }

#ifndef PORT
//#define PORT 2222
#define PORT 22
#endif
static void listen_3(void/*const char * addr*/)
{
    //struct in_addr in = { 0 } ;
    //if (inet_aton(addr, &in) == 0)
    //    die("'%s': not valid ipv4 address", addr);

    int ssd = socket(AF_INET, SOCK_STREAM, 0);
    if (ssd < 0) die("socket:");
    if (ssd != 3) {
        xdup2(ssd, 3);
        close(ssd);
    }
    //atexit(close_3);
    setsockopt(3, SOL_SOCKET, SO_REUSEADDR, (int[]){1}, sizeof (int));
    struct sockaddr_in iaddr = { .sin_family = AF_INET,
                                 .sin_port = IPORT(PORT)/*,
                                 .sin_addr = in*/ };
    if (bind(3, (struct sockaddr*)&iaddr, sizeof iaddr) < 0)
        die("bind:");

    listen(3, 5);
 }

static void run_sshd(void)
{
    struct sockaddr_in peer;
    socklen_t peerlen = sizeof peer;
    int sd = accept(3, (struct sockaddr *)&peer, &peerlen);
    if (sd < 0) {
        msg("accept failed (%d)", errno);
        sleep(1); // are we in trouble ?
        return;
    }
    pid_t pid = fork();
    if (pid != 0) {
        // parent or fail -- haven't ever seen fork failing (fork bomb maeby)
        close(sd);
        return;
    }
    // child
    close(3);

    struct sockaddr_in sockname = { 0 };
    socklen_t addrlen = sizeof sockname;
    if (getsockname(sd, (struct sockaddr *)&sockname, &addrlen) < 0)
        die("getsockname:");
    int usb_2_15 = 0;
    #undef ia_192_168_2_15
    #define ia_192_168_2_15 IADDR(192,168,2,15)
    //#define ia_192_168_2_15_ IADDR(127,0,0,1)
    if (sockname.sin_addr.s_addr == ia_192_168_2_15) {
        // see if rndis has address 192.168.2.15 -- if so use "fixed" hostkey
        struct ifreq ifr = { 0 };
        strcpy(ifr.ifr_name, "rndis0");
        //strcpy(ifr.ifr_name, "lo");
        ioctl(sd, SIOCGIFADDR, &ifr); // if this fails usb_2_15 will stay zero
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align" // hmmm...
        struct sockaddr_in * iaddr = (struct sockaddr_in *)&ifr.ifr_addr;
#pragma GCC diagnostic pop
        if (iaddr->sin_addr.s_addr == ia_192_168_2_15)
            usb_2_15 = 1;
    }
    #undef ia_192_168_2_15

    const char * fmt;
    if (usb_2_15) {// ldpreload-sshdivert.c will use this to divert host key...
        putenv((char*)(intptr_t)"VIA_USB=1"); // ...and (could) any username
        fmt = "connection from %s (usb)";
    }
    else {
        putenv((char*)(intptr_t)"VIA_USB=0");
        fmt = "connection from %s";
    }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    msg(fmt, inet_ntoa(peer.sin_addr));
#pragma GCC diagnostic pop

#ifndef APP_SHARE_DIR
#define APP_SHARE_DIR "/usr/share/mad-developer"
#endif
    putenv((char*)(intptr_t)
           "LD_PRELOAD=" APP_SHARE_DIR "/ldpreload-sshdivert.so");

    const char * args[3];
#if 1
    args[0] = "/usr/sbin/sshd";
    args[1] = "-i";
    args[2] = NULL;
    xdup2(sd, 0);
    close(sd);
    xdup2(0, 1);
    // fd 2 goes upto qml -- saw once "/etc/ssh/sshd_config: Permission denied"
#else
    args[0] = "/usr/bin/env";
    args[1] = NULL;
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnested-externs"
    extern char ** environ;
#pragma GCC diagnostic pop

    execve(args[0], (char**)(intptr_t)args, environ);
    die("execve:");
}


int main(void)
{
    // pkexec ... maeby .. but no
    //if (fcntl(2, F_GETFD) < 0) {
    //    xdup2(8, 0); close(8);
    //    xdup2(9, 2); close(8);
    //}
    listen_3();

    //#define xstr(s) str(s)
    //#define str(s) #s
    //msg("inetsshd listening port " xstr(PORT));
    msg("inetsshd listening: pid %d", getpid());

    signal(SIGCHLD, SIG_IGN);

    struct pollfd pfds[2] = {
        { .fd = 3, .events = POLLIN },
        { .fd = 0, .events = POLLIN }
    };
    while (1) {
        (void)poll(pfds, sizeof pfds / sizeof pfds[0], -1);
        if (pfds[0].revents) {  // new connection to server socket
            run_sshd();
        }
        if (pfds[1].revents) { // message (EOF) from launcher
            char buf[128];
            int l = read(0, buf, sizeof buf);
            if (l <= 0) // The eof //
                exit(0);
        }
    }
    return 0; // not reached //
}
