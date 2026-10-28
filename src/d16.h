/* d16.h - diff16's shared declarations (docs/SPEC.md)
 *
 * diff16 is built two ways: for DOS with Open Watcom and libc16 in the
 * compact model (__WATCOMC__), where data pointers are far, and for the
 * host with a C89 compiler and POSIX, for development and tests.
 */

#ifndef D16_H
#define D16_H

#ifndef VERSION
#define VERSION "dev"
#endif

#ifdef __WATCOMC__
#include <io.h>
#else
#include <unistd.h>
#define O_BINARY 0
#endif
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>                /* SEEK_SET */
#include <stdlib.h>
#include <string.h>

/* 32-bit unsigned, the line key: unsigned long on DOS, where long is 32
 * bits; unsigned int on the hosts we build on, where long may be 64. */
#ifdef __WATCOMC__
typedef unsigned long d16_u32;
#else
typedef unsigned int d16_u32;
#endif

/* Line numbers and counts are long everywhere; LINE_MAX_COUNT bounds them
 * so that sums of two never overflow. */
#define LINE_MAX_COUNT 0x3FFFFFFFL

/* Exit statuses */
#define EXIT_SAME  0
#define EXIT_DIFF  1
#define EXIT_TROUBLE 2

/* ------------------------------------------------------------------ */
/* error.c: diagnostics; every fatal error exits with EXIT_TROUBLE after
 * removing temporary files and a partial output file. */

void err_msg(const char *a, const char *b, const char *c);
void fatal(const char *a, const char *b, const char *c);
void fatal_errno(const char *what, const char *name);
void fatal_nomem(void);
void main_cleanup(void);           /* main.c */

/* ------------------------------------------------------------------ */
/* sys.c: the platform layer */

#define SYS_PATH_MAX 128

void sys_init(void);
int  sys_open_read(const char *path);
int  sys_create_new(const char *path);
int  sys_create_trunc(const char *path);
int  sys_unlink(const char *path);
int  sys_same_file(const char *a, const char *b);
unsigned sys_ticks(void);
int  sys_interrupted(void);
void sys_check_break(void);
const char *sys_getenv(const char *name);

/* ------------------------------------------------------------------ */
/* output.c: buffered output to one handle, with no *printf */

void out_open(int fd, const char *name);
void out_bytes(const char *p, unsigned n);
void out_char(char c);
void out_str(const char *s);
void out_long(long v);
void out_flush(void);

/* write a whole string to standard error, ignoring failure */
void err_str(const char *s);

/* ------------------------------------------------------------------ */
/* temp.c: temporary files, removed on exit */

int  temp_create(void);
void temp_remove_all(void);

/* ------------------------------------------------------------------ */
/* bigarr.c: an append-only array of fixed-size elements in pages, kept
 * in memory while it fits and in a temporary file after that. */

#define BA_PAGE      2048u        /* bytes in a page */
#define BA_MAXRES    256          /* most pages in memory, per array */
#define BA_FRAMES    2            /* page cache frames, per array */

struct bigarr {
    unsigned esize;               /* bytes in an element */
    unsigned shift;               /* log2(elements in a page) */
    unsigned mask;                /* elements in a page - 1 */
    long count;                   /* elements appended */
    unsigned nres;                /* pages 0 .. nres-1 are in memory */
    int spilled;                  /* later pages are in the file */
    int fd;                       /* the temporary file, or -1 */
    long fpages;                  /* pages written to the file */
    unsigned char *res[BA_MAXRES];
    unsigned char *frame[BA_FRAMES];
    long ftag[BA_FRAMES];         /* page in each frame, or -1 */
    unsigned fnext;               /* frame to reuse next */
    unsigned char *tail;          /* page being filled, once spilled */
};

void ba_init(struct bigarr *ba, unsigned esize);
void ba_append(struct bigarr *ba, const void *elem);
void ba_seal(struct bigarr *ba);
const void *ba_slow(struct bigarr *ba, long i);
void ba_free(struct bigarr *ba);

/* Element i, valid until the next ba_ call on the array. */
#define BA_GET(ba, i) \
    (((i) >> (ba)->shift) < (long)(ba)->nres \
     ?(const void *)((ba)->res[(unsigned)((i) >> (ba)->shift)] \
                      + ((unsigned)(i) & (ba)->mask) * (ba)->esize) \
     : ba_slow((ba), (i)))

extern unsigned ba_res_limit;     /* test hook: most resident pages */

/* ------------------------------------------------------------------ */
/* lines.c: reading a file as lines */

#define TERM_NONE 0               /* the last line, with no newline */
#define TERM_LF   1
#define TERM_CRLF 2

#define LR_BUF 4096u

struct lreader {
    int fd;
    const char *name;
    unsigned char *buf;
    unsigned pos, len;
    int eof;
    int inline_;                  /* inside a line */
    int term;                     /* how the line just ended */
    long line;                    /* lines finished */
};

void lr_open(struct lreader *r, const char *name);
void lr_rewind(struct lreader *r);
void lr_close(struct lreader *r);
int  lr_seg(struct lreader *r, const unsigned char **p);
void lr_skip(struct lreader *r, long n);
int  lr_equal(struct lreader *a, struct lreader *b);
int  lr_copy(struct lreader *r);
void lr_scan(struct lreader *r, struct bigarr *keys);

/* ------------------------------------------------------------------ */
/* compare.c: matching, to a list of changes */

/* A change: delete na lines of the old file from line a (0-based) and
 * insert nb lines of the new file from line b. Lines between changes
 * are equal; the list is in order and changes never touch. */
struct change {
    long a, na;
    long b, nb;
};

struct chlist {
    struct bigarr ba;
    struct change last;           /* not yet appended */
    int have_last;
};

void ch_init(struct chlist *cl);
void ch_add(struct chlist *cl, long a, long na, long b, long nb);
void ch_finish(struct chlist *cl);
long ch_count(struct chlist *cl);
void ch_get(struct chlist *cl, long i, struct change *c);

void cmp_init(void);
void cmp_diff(struct bigarr *ka, struct bigarr *kb, struct chlist *out);

extern long cmp_dmax;             /* the edit-distance search limit */
extern long cmp_limited;          /* searches that reached it */

/* ------------------------------------------------------------------ */
/* verify.c: check equal lines byte for byte */

void verify(struct lreader *ra, struct lreader *rb, long na, long nb,
            struct chlist *in, struct chlist *out);

/* ------------------------------------------------------------------ */
/* unified.c: the unified diff */

void unified(struct lreader *ra, struct lreader *rb, long na, long nb,
             struct chlist *cl, const char *la, const char *lb,
             long context);

#endif
