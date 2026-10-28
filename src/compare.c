/* compare.c - matching lines (docs/SPEC.md, "Matching")
 *
 * Lines are compared by key; verify() later checks every pair matched
 * here byte for byte, so a key collision costs diff size, never
 * correctness.
 *
 * The method is the divide-and-conquer form of the O(ND) algorithm in
 * E. W. Myers, "An O(ND) Difference Algorithm and Its Variations",
 * Algorithmica 1 (1986): for a box of the edit graph, after taking off
 * equal lines at both ends, search forward from the top left and back
 * from the bottom right, one edit at a time, until the two searches
 * meet; the snake where they meet splits the box in two. Memory is two
 * arrays of 2 * cmp_dmax + 3 positions and a fixed stack of boxes.
 *
 * Bounds: a search that needs more than cmp_dmax edits each way stops,
 * and the box is split at the point the forward search got furthest
 * along. A box that can't be split (the stack is full) becomes one
 * change. Both give a correct diff, maybe a longer one. cmp_dmax is a
 * constant, not taken from free memory, so the output depends only on
 * the input.
 */

#include "d16.h"

#ifndef CMP_DMAX
#define CMP_DMAX 2048L
#endif
#define STACK_MAX 64

long cmp_dmax = CMP_DMAX;
long cmp_limited;

struct box {
    long x0, x1, y0, y1;
};

static struct bigarr *ka, *kb;
static long *vf, *vb;
static struct box stack[STACK_MAX];

#define KA(i) (*(const d16_u32 *)BA_GET(ka, (i)))
#define KB(i) (*(const d16_u32 *)BA_GET(kb, (i)))

/* forward and reverse furthest points on diagonal k; -1 is none */
#define VF(k) vf[(k) + cmp_dmax + 1]
#define VB(k) vb[(k) - delta + cmp_dmax + 1]

void cmp_init(void)
{
    unsigned n = (unsigned)(2 * cmp_dmax + 3);

    vf = malloc(n * sizeof *vf);
    vb = malloc(n * sizeof *vb);
    if (!vf || !vb)
        fatal_nomem();
}

/* ------------------------------------------------------------------ */
/* the list of changes */

void ch_init(struct chlist *cl)
{
    ba_init(&cl->ba, sizeof(struct change));
    cl->have_last = 0;
}

/* Add a change after the others, joining it to the last if they touch. */
void ch_add(struct chlist *cl, long a, long na, long b, long nb)
{
    struct change *l = &cl->last;

    if (na == 0 && nb == 0)
        return;
    if (cl->have_last && l->a + l->na == a && l->b + l->nb == b) {
        l->na += na;
        l->nb += nb;
        return;
    }
    if (cl->have_last)
        ba_append(&cl->ba, l);
    l->a = a;
    l->na = na;
    l->b = b;
    l->nb = nb;
    cl->have_last = 1;
}

void ch_finish(struct chlist *cl)
{
    if (cl->have_last)
        ba_append(&cl->ba, &cl->last);
    cl->have_last = 0;
    ba_seal(&cl->ba);
}

long ch_count(struct chlist *cl)
{
    return cl->ba.count;
}

void ch_get(struct chlist *cl, long i, struct change *c)
{
    memcpy(c, BA_GET(&cl->ba, i), sizeof *c);
}

/* ------------------------------------------------------------------ */
/* the search */

/* Split box b (with no equal lines at either end, and both sides not
 * empty) at a snake: left gets the box before it, right the box after.
 * 0 if it can't be split. */
static int split(const struct box *b, struct box *left, struct box *right)
{
    long x0 = b->x0, y0 = b->y0;
    long n = b->x1 - b->x0, m = b->y1 - b->y0;
    long delta = n - m;
    int odd = (int)(delta & 1);
    long dlim, d, k, x, y, xs, ys, c;
    long fmin = 0, fmax = 0, bmin = 0, bmax = 0, lo, hi;
    long sx, sy, ex, ey;

    dlim = (n + m + 1) / 2;
    if (dlim > cmp_dmax)
        dlim = cmp_dmax;

    for (d = 0; d <= dlim; d++) {
        sys_check_break();

        /* forward: diagonals k = x - y from -d to d, inside the box */
        lo = -d > -m ? -d : -m;
        hi = d < n ? d : n;
        if ((lo + d) & 1)
            lo++;
        if ((hi + d) & 1)
            hi--;
        for (k = lo; k <= hi; k += 2) {
            if (d == 0)
                x = 0;
            else {
                x = -1;
                /* down from diagonal k + 1 */
                if (k + 1 >= fmin && k + 1 <= fmax) {
                    c = VF(k + 1);
                    if (c >= 0 && c - (k + 1) < m)
                        x = c;
                }
                /* right from diagonal k - 1 */
                if (k - 1 >= fmin && k - 1 <= fmax) {
                    c = VF(k - 1);
                    if (c >= 0 && c < n && c + 1 > x)
                        x = c + 1;
                }
                if (x < 0) {
                    VF(k) = -1;
                    continue;
                }
            }
            y = x - k;
            xs = x;
            ys = y;
            while (x < n && y < m && KA(x0 + x) == KB(y0 + y)) {
                x++;
                y++;
            }
            VF(k) = x;
            if (odd && d > 0 && k >= bmin && k <= bmax
                && VB(k) >= 0 && VB(k) <= x) {
                sx = xs;
                sy = ys;
                ex = x;
                ey = y;
                goto found;
            }
        }
        fmin = lo;
        fmax = hi;

        /* reverse: diagonals from delta - d to delta + d */
        lo = delta - d > -m ? delta - d : -m;
        hi = delta + d < n ? delta + d : n;
        if ((lo - delta + d) & 1)
            lo++;
        if ((hi - delta + d) & 1)
            hi--;
        for (k = lo; k <= hi; k += 2) {
            if (d == 0)
                x = n;
            else {
                x = n + 1;
                /* left from diagonal k + 1 */
                if (k + 1 >= bmin && k + 1 <= bmax) {
                    c = VB(k + 1);
                    if (c > 0)
                        x = c - 1;
                }
                /* up from diagonal k - 1 */
                if (k - 1 >= bmin && k - 1 <= bmax) {
                    c = VB(k - 1);
                    if (c >= 0 && c - (k - 1) > 0 && c < x)
                        x = c;
                }
                if (x > n) {
                    VB(k) = -1;
                    continue;
                }
            }
            y = x - k;
            xs = x;
            ys = y;
            while (x > 0 && y > 0 && KA(x0 + x - 1) == KB(y0 + y - 1)) {
                x--;
                y--;
            }
            VB(k) = x;
            if (!odd && k >= fmin && k <= fmax
                && VF(k) >= 0 && VF(k) >= x) {
                sx = x;
                sy = y;
                ex = xs;
                ey = ys;
                goto found;
            }
        }
        bmin = lo;
        bmax = hi;
    }

    /* Too many edits: split where the forward search got furthest. */
    cmp_limited++;
    sx = -1;
    sy = 0;
    for (k = fmin; k <= fmax; k += 2) {
        x = VF(k);
        if (x >= 0 && (sx < 0 || 2 * x - k > sx + sy)) {
            sx = x;
            sy = x - k;
        }
    }
    if (sx < 0)
        return 0;
    ex = sx;
    ey = sy;

found:
    if (sx < 0 || sy < 0 || ex > n || ey > m || sx > ex || sy > ey)
        return 0;
    left->x0 = x0;
    left->x1 = x0 + sx;
    left->y0 = y0;
    left->y1 = y0 + sy;
    right->x0 = x0 + ex;
    right->x1 = b->x1;
    right->y0 = y0 + ey;
    right->y1 = b->y1;
    /* each part must be smaller, or this would never end */
    if ((sx == 0 && sy == 0 && ex == n && ey == m)
        || (left->x1 == b->x1 && left->y1 == b->y1)
        || (right->x0 == b->x0 && right->y0 == b->y0))
        return 0;
    return 1;
}

/* Compare the keys of the two files, adding the changes to out in order. */
void cmp_diff(struct bigarr *a, struct bigarr *b, struct chlist *out)
{
    struct box bx, left, right;
    int sp = 0;

    ka = a;
    kb = b;
    stack[sp].x0 = 0;
    stack[sp].x1 = a->count;
    stack[sp].y0 = 0;
    stack[sp].y1 = b->count;
    sp++;

    while (sp > 0) {
        bx = stack[--sp];
        /* equal lines at the start and end */
        while (bx.x0 < bx.x1 && bx.y0 < bx.y1 && KA(bx.x0) == KB(bx.y0)) {
            bx.x0++;
            bx.y0++;
        }
        while (bx.x0 < bx.x1 && bx.y0 < bx.y1
               && KA(bx.x1 - 1) == KB(bx.y1 - 1)) {
            bx.x1--;
            bx.y1--;
        }
        if (bx.x0 == bx.x1 || bx.y0 == bx.y1) {
            ch_add(out, bx.x0, bx.x1 - bx.x0, bx.y0, bx.y1 - bx.y0);
            continue;
        }
        if (sp + 2 > STACK_MAX || !split(&bx, &left, &right)) {
            ch_add(out, bx.x0, bx.x1 - bx.x0, bx.y0, bx.y1 - bx.y0);
            continue;
        }
        /* the left part is done first, so changes come out in order */
        stack[sp++] = right;
        stack[sp++] = left;
    }
    ch_finish(out);
}
