/* lines.c - reading a file as lines (docs/SPEC.md, "Lines")
 *
 * A line is its bytes and how it ends: LF, CR LF, or nothing (the last
 * line of a file with no final newline). A CR not followed by LF is part
 * of the line. Lines are returned in segments, so a line may be any
 * length; a NUL byte anywhere is an error (binary files are not
 * compared). Files are read in binary, and always from the start, in
 * order.
 */

#include "d16.h"

void lr_open(struct lreader *r, const char *name)
{
    memset(r, 0, sizeof *r);
    r->name = name;
    r->buf = malloc(LR_BUF);
    if (!r->buf)
        fatal_nomem();
    r->fd = sys_open_read(name);
    if (r->fd < 0)
        fatal_errno("cannot open", name);
}

void lr_rewind(struct lreader *r)
{
    if (lseek(r->fd, 0L, SEEK_SET) < 0)
        fatal_errno("seek error", r->name);
    r->pos = r->len = 0;
    r->eof = r->inline_ = 0;
    r->line = 0;
}

void lr_close(struct lreader *r)
{
    close(r->fd);
    r->fd = -1;
}

/* Read more, keeping the unread bytes (at most one, a CR). */
static void fill(struct lreader *r)
{
    unsigned keep = r->len - r->pos;
    int n;

    if (keep)
        r->buf[0] = r->buf[r->pos];
    r->pos = 0;
    r->len = keep;
    if (r->eof)
        return;
    sys_check_break();
    n = read(r->fd, r->buf + keep, LR_BUF - keep);
    if (n < 0)
        fatal_errno("read error", r->name);
    if (n == 0)
        r->eof = 1;
    r->len += (unsigned)n;
}

/* The next segment of the current line: its length (> 0), with *p at its
 * bytes, valid until the next call. 0 at the end of the line, with
 * r->term saying how it ended. -1 when there are no more lines. */
int lr_seg(struct lreader *r, const unsigned char **p)
{
    const unsigned char *b;
    unsigned i, end, n;

    for (;;) {
        if (r->pos == r->len)
            fill(r);
        if (r->pos == r->len) {
            if (!r->inline_)
                return -1;
            r->inline_ = 0;
            r->term = TERM_NONE;
            r->line++;
            return 0;
        }
        r->inline_ = 1;
        b = r->buf;
        i = r->pos;
        while (i < r->len && b[i] != '\n' && b[i] != 0)
            i++;
        if (i < r->len && b[i] == 0)
            fatal(r->name, "binary file (it contains a NUL byte)", NULL);
        if (i < r->len) {
            /* a newline at i */
            end = i;
            if (end > r->pos && b[end - 1] == '\r')
                end--;
            if (end > r->pos) {
                *p = b + r->pos;
                n = end - r->pos;
                r->pos = end;
                return (int)n;
            }
            r->term = end < i ? TERM_CRLF : TERM_LF;
            r->pos = i + 1;
            r->inline_ = 0;
            r->line++;
            return 0;
        }
        /* No newline here. A CR at the end may start a CR LF: keep it
         * until the next byte is known. */
        end = r->len;
        if (!r->eof && b[end - 1] == '\r')
            end--;
        if (end > r->pos) {
            *p = b + r->pos;
            n = end - r->pos;
            r->pos = end;
            return (int)n;
        }
        fill(r);
    }
}

static void changed(struct lreader *r)
{
    fatal(r->name, "file changed while being compared", NULL);
}

/* Skip n lines. */
void lr_skip(struct lreader *r, long n)
{
    const unsigned char *p;
    int k;

    while (n-- > 0) {
        while ((k = lr_seg(r, &p)) > 0)
            ;
        if (k < 0)
            changed(r);
    }
}

/* Read a line from each; 1 if they are equal: the same bytes, and either
 * both or neither ending with a newline (LF and CR LF are equal). */
int lr_equal(struct lreader *a, struct lreader *b)
{
    const unsigned char *pa = NULL, *pb = NULL;
    int la = 0, lb = 0, n, same = 1;

    for (;;) {
        if (la == 0) {
            la = lr_seg(a, &pa);
            if (la < 0)
                changed(a);
        }
        if (lb == 0) {
            lb = lr_seg(b, &pb);
            if (lb < 0)
                changed(b);
        }
        if (la == 0 || lb == 0)
            break;
        n = la < lb ? la : lb;
        if (same && memcmp(pa, pb, (unsigned)n) != 0)
            same = 0;
        pa += n;
        pb += n;
        la -= n;
        lb -= n;
    }
    /* finish whichever line is longer */
    if (la != 0 || lb != 0) {
        same = 0;
        if (la != 0)
            lr_skip(a, 1);
        else
            lr_skip(b, 1);
    }
    if ((a->term == TERM_NONE) != (b->term == TERM_NONE))
        same = 0;
    return same;
}

/* Write a line to the output with an LF; how it ended. */
int lr_copy(struct lreader *r)
{
    const unsigned char *p;
    int k;

    while ((k = lr_seg(r, &p)) > 0)
        out_bytes((const char *)p, (unsigned)k);
    if (k < 0)
        changed(r);
    out_char('\n');
    return r->term;
}

/* Append a key for every line of the file to keys. The key is a hash of
 * the line's bytes, length and whether it ends with a newline: equal
 * lines have equal keys. Two 16-bit halves keep it quick on an 8088. */
void lr_scan(struct lreader *r, struct bigarr *keys)
{
    const unsigned char *p;
    unsigned h1, h2, len, i;
    d16_u32 key;
    int k;

    lr_rewind(r);
    for (;;) {
        h1 = 0x2B1Fu;
        h2 = 0;
        len = 0;
        while ((k = lr_seg(r, &p)) > 0) {
            for (i = 0; i < (unsigned)k; i++) {
                h1 = (((h1 << 5) | (h1 >> 11)) ^ p[i]) & 0xFFFFu;
                h2 = ((h2 << 5) - h2 + p[i]) & 0xFFFFu;
            }
            len = (len + (unsigned)k) & 0xFFFFu;
        }
        if (k < 0)
            break;
#ifdef D16_WEAK_HASH
        /* test build: collide often, to exercise verify() */
        h1 = 0;
        h2 &= 3;
#else
        h1 ^= len;
#endif
        if (r->term == TERM_NONE)
            h2 ^= 0x5A5Au;
        key = ((d16_u32)(h1 & 0xFFFFu) << 16) | (h2 & 0xFFFFu);
        ba_append(keys, &key);
    }
    ba_seal(keys);
}
