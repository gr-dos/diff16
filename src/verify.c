/* verify.c - checking matched lines byte for byte (docs/SPEC.md,
 * "Verification")
 *
 * compare.c matches lines by key. Here both files are read once more, in
 * order, and every pair of lines it left equal is compared byte for
 * byte. A pair that differs (a key collision) becomes a change of its
 * own, deleting the one line and inserting the other, which is always a
 * correct edit. Only the list that comes out of here is printed.
 */

#include "d16.h"

void verify(struct lreader *ra, struct lreader *rb, long na, long nb,
            struct chlist *in, struct chlist *out)
{
    struct change c;
    long i, n = ch_count(in), ca = 0, cb = 0, run, t;
    const unsigned char *p;

    lr_rewind(ra);
    lr_rewind(rb);
    for (i = 0; i <= n; i++) {
        if (i < n)
            ch_get(in, i, &c);
        else {
            c.a = na;
            c.na = 0;
            c.b = nb;
            c.nb = 0;
        }
        run = c.a - ca;
        if (run < 0 || run != c.b - cb)
            fatal("internal error", "bad change list", NULL);
        for (t = 0; t < run; t++) {
            if (!lr_equal(ra, rb))
                ch_add(out, ca + t, 1, cb + t, 1);
            if ((t & 63) == 63)
                sys_check_break();
        }
        if (i == n)
            break;
        lr_skip(ra, c.na);
        lr_skip(rb, c.nb);
        ch_add(out, c.a, c.na, c.b, c.nb);
        ca = c.a + c.na;
        cb = c.b + c.nb;
    }
    ch_finish(out);
    /* both files must have ended where the scan said */
    if (lr_seg(ra, &p) >= 0)
        fatal(ra->name, "file changed while being compared", NULL);
    if (lr_seg(rb, &p) >= 0)
        fatal(rb->name, "file changed while being compared", NULL);
}
