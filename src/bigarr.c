/* bigarr.c - append-only arrays kept in memory while they fit, then in a
 * temporary file (docs/SPEC.md, "Memory")
 *
 * Pages are BA_PAGE bytes. The first pages are allocated from the heap
 * as the array grows; when an allocation fails (or the array has
 * ba_res_limit pages in memory) every later page goes to a temporary
 * file instead, written once in order. Pages in the file are read back
 * through BA_FRAMES frames allocated by ba_init, so reading never needs
 * more memory. Which pages are where never changes the program's output.
 */

#include "d16.h"

#ifndef BA_RES_LIMIT
#define BA_RES_LIMIT BA_MAXRES
#endif

unsigned ba_res_limit = BA_RES_LIMIT;

void ba_init(struct bigarr *ba, unsigned esize)
{
    unsigned per = BA_PAGE / esize;
    int f;

    memset(ba, 0, sizeof *ba);
    ba->esize = esize;
    while ((1u << ba->shift) < per)
        ba->shift++;
    if ((1u << ba->shift) != per || per * esize != BA_PAGE)
        fatal("internal error", "bad element size", NULL);
    ba->mask = per - 1;
    ba->fd = -1;
    for (f = 0; f < BA_FRAMES; f++) {
        ba->frame[f] = malloc(BA_PAGE);
        if (!ba->frame[f])
            fatal_nomem();
        ba->ftag[f] = -1;
    }
}

static void write_page(struct bigarr *ba, const unsigned char *p)
{
    int w;

    if (ba->fd < 0)
        ba->fd = temp_create();
    w = write(ba->fd, p, BA_PAGE);
    if (w != (int)BA_PAGE) {
        if (w >= 0)
            errno = ENOSPC;
        fatal_errno("write error", "temporary file");
    }
    ba->fpages++;
}

void ba_append(struct bigarr *ba, const void *elem)
{
    unsigned off = ((unsigned)ba->count & ba->mask) * ba->esize;
    unsigned char *p;

    if (ba->count >= LINE_MAX_COUNT)
        fatal("too many lines", NULL, NULL);
    if (!ba->spilled) {
        if (off == 0) {
            p = NULL;
            if (ba->nres < BA_MAXRES && ba->nres < ba_res_limit)
                p = malloc(BA_PAGE);
            if (p)
                ba->res[ba->nres++] = p;
            else {
                ba->spilled = 1;
                ba->tail = ba->frame[0];
            }
        }
    }
    if (!ba->spilled)
        p = ba->res[ba->nres - 1];
    else
        p = ba->tail;
    memcpy(p + off, elem, ba->esize);
    ba->count++;
    if (ba->spilled && off + ba->esize == BA_PAGE)
        write_page(ba, ba->tail);
}

/* No more appends: write a partly filled last page. */
void ba_seal(struct bigarr *ba)
{
    if (ba->spilled && (ba->count & ba->mask) != 0)
        write_page(ba, ba->tail);
    ba->tail = NULL;
}

/* Element i of a sealed array, from a page in the file. */
const void *ba_slow(struct bigarr *ba, long i)
{
    long page;
    unsigned f;
    int n;

    if (i < 0 || i >= ba->count)
        fatal("internal error", "index out of range", NULL);
    page = i >> ba->shift;
    if (page < (long)ba->nres)
        return ba->res[(unsigned)page] + ((unsigned)i & ba->mask) * ba->esize;
    for (f = 0; f < BA_FRAMES; f++)
        if (ba->ftag[f] == page)
            break;
    if (f == BA_FRAMES) {
        f = ba->fnext;
        ba->ftag[f] = -1;
        if (lseek(ba->fd, (page - ba->nres) * (long)BA_PAGE, SEEK_SET) < 0)
            fatal_errno("seek error", "temporary file");
        n = read(ba->fd, ba->frame[f], BA_PAGE);
        if (n < 0)
            fatal_errno("read error", "temporary file");
        if (n != (int)BA_PAGE)
            fatal("temporary file", "read error", "file too short");
        ba->ftag[f] = page;
    }
    /* the frame least recently used goes next */
    ba->fnext = (f + 1) % BA_FRAMES;
    return ba->frame[f] + ((unsigned)i & ba->mask) * ba->esize;
}

void ba_free(struct bigarr *ba)
{
    unsigned u;
    int f;

    for (u = 0; u < ba->nres; u++)
        free(ba->res[u]);
    for (f = 0; f < BA_FRAMES; f++)
        free(ba->frame[f]);
    ba->nres = 0;
    ba->count = 0;
}
