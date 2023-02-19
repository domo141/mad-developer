#if 0 /* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*-
 set -euf
 case ${1-} in *'/'*) trg=$1; shift ;; *) trg=${0##*''/}; trg=${trg%.c}.so
 esac; test -e "$trg" && rm "$trg"
 case ${1-} in '') set x -O2; shift; esac
 x_exec () { printf %s\\n "$*" >&2; exec "$@"; }
 x_exec ${CC:-gcc} -std=c11 -shared -fPIC -o "$trg" "$0" $@ -ldl
 exit $?
 */
#endif
/*
 * $ ldpreload-sshdivert.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *	Copyright (c) 2023 Tomi Ollila
 *	    All rights reserved
 *
 * Created: Sat 14 Jan 2023 21:15:50 +0200 too
 * Last modified: Mon 30 Jan 2023 19:34:42 +0200 too
 */
// SPDX-License-Identifier: BSD 2-Clause "Simplified" License

#include "more-warnings.h"

#define _GNU_SOURCE // for RTLD_NEXT

/* misc notes written during development, kept for future ref (to self) */

/* used ldpreload-log-statopen-973.so (from log-dlproc-life repo) to log
 * calls, then grepped passwd, shadow and ssh_host_ to find matched functions
 * ... then again, sfos listed other files than fedora/debian */

/* should have done ltrace (also) at the beginning */

#define open open_hidden
#define openat openat_hidden
#define fopen fopen_hidden
// next (...two) used in 32bit armv7hl -- seen tnx to ltrace...
#define open64 open64_hidden
#define fopen64 fopen64_hidden

/* it could be all includes below not needed,
   list inherited from ldpreload-log-statopen.c */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h> // offsetof()
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <time.h> // whenever clock_gettime() is used

#include <sys/socket.h>
#include <sys/un.h>

#include <dlfcn.h>

#undef open
#undef openat
#undef fopen
#undef open64
#undef fopen64


static void * dlsym_next(const char * symbol)
{
    void * sym = dlsym(RTLD_NEXT, symbol);
    char * str = dlerror();

    if (str != NULL) {
	fprintf(stderr, "finding symbol '%s' failed: %s", symbol, str);
	exit(1);
    }
    return sym;
}

/*
 * this used to be open_next() and it called in functions, in SFOS 4.4.0.72
 * (w/ patchmanager installed), there were interesting interactions with
 * patchmanager, and newstatat calls to ensure none of the path components
 * were symlinks (whether sfos glibc or patchmanager did that...). using
 * openat() strace(1) showed less "interactions"...
 */
int (*openat_next)(int dirfd, const char * pathname, int flags, mode_t mode);

__attribute__((constructor))
static void find_openat(void)
{
    *(void**) (&openat_next) = dlsym_next("openat");
}

#if 0
#define cprintf(...) fprintf(stderr, __VA_ARGS__)
#else
#define cprintf(...) do {} while (0)
#endif

// naming based what is seen in notmuch source code
#define STRCMP_LITERAL(var, literal) memcmp((var), (literal), sizeof (literal))

// like in notmuch, strncmp() replaced w/ memcmp()
#define STRNCMP_LITERAL(var, literal) \
    memcmp((var), (literal), sizeof (literal) - 1)


#define HETC "/etc/ssh"
#ifndef APP_SHARE_DIR
#define APP_SHARE_DIR "/usr/share/mad-developer"
#endif
#define HOST_KEY_DIR APP_SHARE_DIR "/etc"
#define FPASSWD "/etc/passwd"
#define FSHADOW "/etc/shadow"


static inline int open_(const char * pathname, int flags, mode_t mode)
{
    return openat_next(AT_FDCWD, pathname, flags, mode);
}

static int via_usb_(void)
{
    static int via_usb = -1;
    if (via_usb < 0) {
	const char * ev = getenv("VIA_USB");
	via_usb = (ev != NULL && ev[0] == '1');
    }
    return via_usb;
}

#if __STDC_VERSION__ < 202300L //202311L ?
// c23 (single argument) static assert to c11 (fyi: gcc9 knows this already)
#undef static_assert
#define static_assert(x) _Static_assert(x, #x)
#endif

static char ruser[36];
static char luser[12]; // 'defaultuser' is 11 in length (nemo 4)

static int xpwdf(const char * pathname)
{
    // this block could be moved until file read, and have only one file
    // open at a time...
    char tfn[] = "/tmp/q.XXXXXXXX";
    static_assert(sizeof tfn == 16);
#if 0
    int fd = mkstemp(tfn);
    int ul = unlink(tfn);
#else
    // the faster, fun part -- not good for hashing, but perhaps for tempfile..
    int fd, ul;
    for (int i = 0; i < 5; i++) {
	struct timespec tp;
	if (clock_gettime(CLOCK_REALTIME, &tp) != 0) return -1;
	uint32_t hv = (uint32_t)tp.tv_sec ^ (uint32_t)(tp.tv_nsec << 2);
	snprintf(tfn + 7, 9, "%08x", hv);
	fd = open_(tfn, O_RDWR|O_CREAT|O_EXCL, 0600);
	// ^v O_TMPFILE too new (or hard) v^ //
	ul = unlink(tfn);
	if (fd >= 0) break;
    }
    // printf("tfn: %s\n", tfn);
#endif
    if (fd < 0) return -1;
    if (ul < 0) goto _fail1;

    int rfd = open_(pathname, O_RDONLY, 0);
    if (rfd < 0) goto _fail1;

    struct stat st;
    if (fstat(rfd, &st) < 0) goto _fail2;
    if ((st.st_mode & S_IFMT) != S_IFREG) { errno = EBADFD; goto _fail2; }
    if (st.st_size >= 16380) { errno = EFBIG; goto _fail2; }

    char buf[16384];
    int len, tlen = 0;
    while ((len = read(rfd, buf + tlen, sizeof buf - tlen)) > 0) {
	// korjaa yhteen writeen, ja testi että mätsää pituuteen
	if (write(fd, buf + tlen, len) < 0) {
	    goto _fail2;
	}
	tlen += len;
    }
    close(rfd);
    if (tlen != st.st_size) { errno = EIO; goto _fail1; }

    if (buf[tlen - 1] != '\n') {
	if (write(fd, "\n", 1) != 1) goto _fail1;
    }
    buf[tlen] = '\0'; // for next while loop below (to ensure exit)
    // find 'defaultuser' or 'nemo', hint other (p[-1] later) //
    char * p = buf;
    while (1) { // hmm forgot strncmp_literal already :O (or "clearer" without)
	if (memcmp(p, "defaultuser:", 12) == 0) { p += 11; break; }
	if (memcmp(p, "nemo:", 5) == 0) { p += 4; break; }
	p = strchr(p, '\n');
	if (p == NULL) break;
	p += 1;
    }
    if (p == NULL) { // no expected user (unlikely in the use case)
	close(fd);
	luser[0] = ruser[0] = '\0';
	return -3; // original file to be opened
    }

    buf[tlen] = '\n'; // change \0 to newline -- just if file did not have it
    const int pl = strchr(p, '\n') - p + 1; // ...for this to match for sure...
    //printf("%d: pl: %d\n", __LINE__, pl);
    if (pl > 512) { errno = E2BIG; goto _fail1; }

    // store pwent data at the beginning of buffer
    memmove(buf, p, pl); // overlap highly unlikely but...
    char * const xp = buf + ((pl + 3) & 0xfffc);
    /*
     * add ruser to passwd/shadow file, copying line after ':' (if ruser set)
     */
    if (p[-1] == 'o') {
	memcpy(luser, "nemo", 5);
	int l = strlen(ruser);
	if (l > 0 && (l != 4 || memcmp(ruser, "nemo", 5) != 0)) {
	    memcpy(xp, ruser, l);
	    p = xp + l;
	}
	else p = NULL;
    }
    else {
	memcpy(luser, "defaultuser", 12);
	int l = strlen(ruser);
	if (l > 0 && (l != 11 || memcmp(ruser, "defaultuser", 12) != 0)) {
	    memcpy(xp, ruser, l);
	    p = xp + l;
	}
	else p = NULL;
    }
    /* copy pwent data after username (ruser) to the file if ruser set... */
    if (p) {
	memcpy(p, buf, pl); p += pl;
	/* ... and write the line, as there is new data to be appended */
	if (write(fd, xp, p - xp) != p - xp) goto _fail1;
    }

    if (lseek(fd, 0, 0) < 0) goto _fail1; // 0 is "idiomatic" SEEK_SET (?)
    // all ok //
    (void)fchmod(fd, 0); // should anyone ask (shadow!)
    return fd;

_fail2: close(rfd);
_fail1: close(fd);
    return -1;
}

/*
 * returns:
 *   -3 if no change, original call passthru
 *   -2 path has changed, caller uses path in buf256
 *   -1 an error when trying to create diverted content
 *  >=0 content in "O_TMPFILE", rv fd. caller returns fd or fdopen()s it
 */
static int divert(const char * pathname, char * buf256)
{
    if (STRNCMP_LITERAL(pathname, HETC "/ssh_host_") == 0) {
	if (! via_usb_())
	    return -3;

	int len = snprintf(buf256, 256,
			   HOST_KEY_DIR "/%s", pathname + sizeof HETC);
	if (len < 256)
	    return -2;
	return -3;
    }
    if (STRCMP_LITERAL(pathname, FPASSWD) == 0 ||
	STRCMP_LITERAL(pathname, FSHADOW) == 0) {
	return xpwdf(pathname);
    }
    return -3;
}


// CPP Macros FTW! -- use gcc -E to examine expansion

#define _deffn(_rt, _fn, _args) \
_rt _fn _args; \
_rt _fn _args { \
    static _rt (*_fn##_next) _args = NULL; \
    if (! _fn##_next ) *(void**) (&_fn##_next) = dlsym_next(#_fn); \
    //const char * fn = #_fn; (void)fn;


_deffn( int, open, (const char * pathname, int flags, mode_t mode) )
#if 0
{
#endif
    cprintf("*** open(\"%s\", %x, %o)\n", pathname, flags, mode);
    char buf256[256];
    int status = divert(pathname, buf256);
    if (status == -3) return open_next(pathname, flags, mode); // most often
    if (status == -2) return open_next(buf256, flags, mode);
    return status;
}

_deffn( int, open64, (const char * pathname, int flags, mode_t mode) )
#if 0
{
#endif
    cprintf("*** open64(\"%s\", %x, %o)\n", pathname, flags, mode);
    char buf256[256];
    int status = divert(pathname, buf256);
    if (status == -3) return open64_next(pathname, flags, mode); // most often
    if (status == -2) return open64_next(buf256, flags, mode);
    return status;
}

int openat(int dirfd, const char * pathname, int flags, mode_t mode);
int openat(int dirfd, const char * pathname, int flags, mode_t mode)
{
    /* "constructor" created openat_next() */
    cprintf("*** openat(%d \"%s\", %x, %o)\n", dirfd, pathname, flags, mode);
    char buf256[256];
    int status = divert(pathname, buf256); // difd is AT_FDCWD, anyway...
    if (status == -3) return openat_next(dirfd, pathname, flags, mode); // most
    if (status == -2) return openat_next(dirfd, buf256, flags, mode);
    return status;
}

/* */

_deffn ( FILE *, fopen, (const char * pathname, const char * mode) )
#if 0
{
#endif
    cprintf("*** fopen(\"%s\", \"%s\")\n", pathname, mode);
    char buf256[256];
    int status = divert(pathname, buf256);
    if (status == -3) return fopen_next(pathname, mode); // most often
    if (status == -2) return fopen_next(buf256, mode);
    if (status == -1) return NULL; // fdopen(-1, "r") would change errno
    return fdopen(status, "r");
}

_deffn ( FILE *, fopen64, (const char * pathname, const char * mode) )
#if 0
{
#endif
    cprintf("*** fopen64(\"%s\", \"%s\")\n", pathname, mode);
    char buf256[256];
    int status = divert(pathname, buf256);
    if (status == -3) return fopen64_next(pathname, mode); // most often
    if (status == -2) return fopen64_next(buf256, mode);
    if (status == -1) return NULL; // fdopen(-1, "r") would change errno
    // divert doesn't do O_LARGEFILE -- does not matter when divert opened it
    return fdopen(status, "r");
}


/* */

// c locale "version" of isalnum() (but lowercase only)
static inline int x_alnum(int c)
{
    return ((c >= '0') && (c <= '9')) || ((c >= 'a') && (c <= 'z'));
}
// c locale "version" of isspace()
static inline int x_space(int c)
{
    return (c == ' '  || c == '\n' || c == '\t' ||
	    c == '\r' || c == '\f' || c == '\v');
}
// c locale opposite of isspace()
static inline int nospace(int c)
{
    return (c != ' '  && c != '\n' && c != '\t' &&
	    c != '\r' && c != '\f' && c != '\v');
}

static int in_loginnames(const char * name)
{
    static int checked = -1;
    if (checked >= 0) return checked;

    // inherited from ldpreload-wrapopen.c... //
    int rfd = open_(APP_SHARE_DIR "/loginnames", O_RDONLY, 0);

    if (rfd < 0) goto not;

    char buf[256];
    int xl = read(rfd, buf, sizeof(buf));
    close(rfd);

    if (xl >= 255) { // max len - 1 -- drop last, possibly partial, name
	for (xl = 253; xl > 0; xl--) { // safer w/ 253, very unlikely case
	    if (x_space(buf[xl])) break;
	}
    }
    if (xl <= 0) goto not;
    buf[xl++] = ' '; // trailing space simplifies scan a bit (maeby)

    const char * r = buf;
    while (xl > 0) {
	if (nospace(*r)) break;
	xl--; r++;
    }
    const int nl = strlen(name);
    const char * rr = r;
    while (xl-- > 0) {
	if (x_alnum(*rr)) { rr++; continue; }
	if (nospace(*rr)) goto not; // stop on 1st "error"
	const int rl = rr - r;
	// printf("%d %d %.*s\n", rl, nl, rl, r);
	if (rl == nl && memcmp(name, r, nl) == 0) {
	    checked = 1;
	    return 1;
	}
	while (xl > 0) {
	    if (nospace(*++rr)) break;
	    xl--;
	}
	r = rr;
    }
not:
    checked = 0;
    return 0;
}

/* avoiding inclusion of <pwd.h> (as it would declare getpwnam()
 * -- therefore just declaring the prototype of getpwuid() here
 */
extern struct passwd *getpwuid(uid_t uid);

/* FYI: ltrace(1) hinted to create these functions...*/

_deffn ( struct passwd *, getpwnam, (const char *name) )
#if 0
{
#endif
    cprintf("*** getpwnam(%s)\n", name);
    if (STRCMP_LITERAL(name, "sshd") == 0)
	return getpwnam_next(name);
    /* in case of commit coming from usb interface, map any user to 100'000 */
    if (/* via_usb_() || \-> ... revert that, not everyone */
	STRCMP_LITERAL(name, "defaultuser") == 0 || // or these 2 cases...
	STRCMP_LITERAL(name, "nemo") == 0 ||
	in_loginnames(name)) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
	strncpy(ruser, name, sizeof ruser);
#pragma GCC diagnostic pop
	if (ruser[sizeof ruser - 1] != '\0') // disable, if too long
	    ruser[0] = ruser[sizeof ruser - 1] = 0;
	else
	    return getpwuid(100 * 1000);
    }
    // else
    return getpwnam_next(name);
}

_deffn ( int, getgrouplist, (const char *user, gid_t group,
			     gid_t *groups, int *ngroups) )
#if 0
{
#endif
    cprintf("*** getgrouplist(%s, %d, ...)\n", user, group);
    /* did not see this call w/ "sshd" happening, but anyway... */
    if (STRCMP_LITERAL(user, "sshd") == 0)
	return getgrouplist_next (user, group, groups, ngroups);

    //fprintf(stderr, ">>> %s: %s %d %d\n", __func__, user, group, *ngroups);
    if (strcmp(user, ruser) == 0) user = luser;
    int rv = getgrouplist_next (user, group, groups, ngroups);
    //fprintf(stderr, "<<< %s: %s %d %d\n", __func__, user, group, *ngroups);
    return rv;
}
