/* output.c - buffered output with no *printf */

#include "d16.h"

#define OUT_BUF 2048u

static char obuf[OUT_BUF];
static unsigned olen;
static int ofd = -1;
static const char *oname;

void out_open(int fd, const char *name)
{
    ofd = fd;
    oname = name;
    olen = 0;
}

void out_flush(void)
{
    unsigned n = olen;
    int w;

    olen = 0;
    if (n == 0)
        return;
    w = write(ofd, obuf, n);
    if (w != (int)n) {
        if (w >= 0)
            errno = ENOSPC;     /* a short write: the disk is full */
        fatal_errno("write error", oname);
    }
}

void out_bytes(const char *p, unsigned n)
{
    unsigned k;

    while (n) {
        if (olen == OUT_BUF)
            out_flush();
        k = OUT_BUF - olen;
        if (k > n)
            k = n;
        memcpy(obuf + olen, p, k);
        olen += k;
        p += k;
        n -= k;
    }
}

void out_char(char c)
{
    if (olen == OUT_BUF)
        out_flush();
    obuf[olen++] = c;
}

void out_str(const char *s)
{
    out_bytes(s, (unsigned)strlen(s));
}

/* v in decimal; v is never negative here */
void out_long(long v)
{
    char d[12];
    int i = 12;

    do {
        d[--i] = (char)('0' + (int)(v % 10));
        v /= 10;
    } while (v && i);
    out_bytes(d + i, (unsigned)(12 - i));
}

void err_str(const char *s)
{
    if (write(2, s, (unsigned)strlen(s)) < 0)
        return;
}
