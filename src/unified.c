/* unified.c - writing the unified diff (docs/SPEC.md, "Output")
 *
 * Changes whose context would touch or overlap (no more than 2 * context
 * equal lines apart) share a hunk. The hunk's ranges are known from the
 * change list before any line is written; then the lines are copied from
 * the two files, read in order: context and deleted lines from the old
 * file, inserted lines from the new one.
 */

#include "d16.h"

/* "start,count" of a hunk range; start is 1-based, or for an empty range
 * the line before it; ",1" is left out. */
static void range(long start, long count)
{
    if (count == 1) {
        out_long(start + 1);
        return;
    }
    out_long(count == 0 ? start : start + 1);
    out_char(',');
    out_long(count);
}

static void line(char prefix, struct lreader *r)
{
    out_char(prefix);
    if (lr_copy(r) == TERM_NONE)
        out_str("\\ No newline at end of file\n");
}

void unified(struct lreader *ra, struct lreader *rb, long na, long nb,
             struct chlist *cl, const char *la, const char *lb,
             long context)
{
    struct change c, last;
    long n = ch_count(cl), i, j, t;
    long as, ae, bs, be, pa = 0, pb = 0;

    (void)nb;
    if (n == 0)
        return;
    lr_rewind(ra);
    lr_rewind(rb);
    out_str("--- ");
    out_str(la);
    out_str("\n+++ ");
    out_str(lb);
    out_char('\n');

    for (i = 0; i < n; i = j + 1) {
        /* the changes of this hunk: i to j */
        ch_get(cl, i, &c);
        as = c.a > context ? c.a - context : 0;
        bs = c.b - (c.a - as);
        last = c;
        for (j = i; j + 1 < n; j++) {
            ch_get(cl, j + 1, &c);
            if (c.a - (last.a + last.na) > 2 * context)
                break;
            last = c;
        }
        ae = last.a + last.na + context;
        if (ae > na)
            ae = na;
        be = last.b + last.nb + (ae - (last.a + last.na));

        out_str("@@ -");
        range(as, ae - as);
        out_str(" +");
        range(bs, be - bs);
        out_str(" @@\n");

        lr_skip(ra, as - pa);
        lr_skip(rb, bs - pb);
        pa = as;
        pb = bs;
        for (t = i; t <= j; t++) {
            ch_get(cl, t, &c);
            for (; pa < c.a; pa++, pb++) {
                line(' ', ra);
                lr_skip(rb, 1);
            }
            for (; pa < c.a + c.na; pa++)
                line('-', ra);
            for (; pb < c.b + c.nb; pb++)
                line('+', rb);
            sys_check_break();
        }
        for (; pa < ae; pa++, pb++) {
            line(' ', ra);
            lr_skip(rb, 1);
        }
    }
}
