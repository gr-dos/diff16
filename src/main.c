/* main.c - diff16: compare two text files, writing a unified diff
 * (docs/SPEC.md)
 */

#include "d16.h"

#define CONTEXT_MAX 99

static const char usage_text[] =
    "diff16 " VERSION " - compare text files, writing a unified diff\n"
    "\n"
    "usage: diff16 [-u] [-U N] [-o FILE] [--] OLD NEW\n"
    "\n"
    "  -u       unified format (the default, and the only format)\n"
    "  -U N     N lines of context, 0 to 99 (default 3)\n"
    "  -o FILE  write the diff to FILE, not standard output\n"
    "  -h       show this help\n"
    "  --       the next arguments are file names, even if they start with -\n"
    "\n"
    "Lines ending in LF and in CR LF compare equal; the diff uses LF.\n"
    "Exit status: 0 no differences, 1 differences, 2 trouble.\n";

static const char *outname;
static int outfd = -1;

static struct lreader ra, rb;
static struct bigarr ka, kb;
static struct chlist raw, ver;

/* On a fatal error (error.c): a partial output file is removed. This is
 * called directly, not through a pointer: with main.obj linked first a
 * static function here may be at offset 0 of the code segment, and a
 * near pointer to it would be a null pointer. */
void main_cleanup(void)
{
    if (outfd >= 0) {
        close(outfd);
        outfd = -1;
        sys_unlink(outname);
    }
}

static void usage_error(const char *a, const char *b)
{
    err_msg(a, b, NULL);
    err_str("Try 'diff16 -h' for help.\n");
    exit(EXIT_TROUBLE);
}

/* The value of -U: decimal digits, 0 to CONTEXT_MAX. */
static long parse_context(const char *s)
{
    long v = 0;

    if (!*s)
        usage_error("invalid context length", s);
    for (; *s; s++) {
        if (*s < '0' || *s > '9')
            usage_error("invalid context length", s);
        v = v * 10 + (*s - '0');
        if (v > CONTEXT_MAX)
            usage_error("context length too large (0 to 99)", NULL);
    }
    return v;
}

/* A file name as a header label: '\' becomes '/', and a name with a
 * space or a control character can't be written safely, so it is an
 * error. The label is made in place. */
static void make_label(char *s, const char *orig)
{
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c <= ' ' || c == 0x7F)
            fatal(orig, "file name can't be written in a patch header", NULL);
        if (c == '\\')
            *s = '/';
    }
}

static char *copy_string(const char *s)
{
    unsigned n = (unsigned)strlen(s) + 1;
    char *p = malloc(n);

    if (!p)
        fatal_nomem();
    memcpy(p, s, n);
    return p;
}

int main(int argc, char **argv)
{
    const char *names[2];
    char *la, *lb;
    long context = 3;
    int nnames = 0, opts = 1, i, same;
    const char *a, *v;

    sys_init();

    for (i = 1; i < argc; i++) {
        a = argv[i];
        if (opts && a[0] == '-' && a[1]) {
            if (strcmp(a, "--") == 0)
                opts = 0;
            else if (strcmp(a, "-u") == 0)
                ;
            else if (strcmp(a, "-h") == 0 || strcmp(a, "-?") == 0
                     || strcmp(a, "--help") == 0) {
                out_open(1, "standard output");
                out_str(usage_text);
                out_flush();
                return EXIT_SAME;
            } else if (a[1] == 'U' || a[1] == 'o') {
                v = a + 2;
                if (!*v) {
                    if (++i == argc)
                        usage_error("option needs a value", a);
                    v = argv[i];
                }
                if (a[1] == 'U')
                    context = parse_context(v);
                else
                    outname = v;
            } else
                usage_error("unknown option", a);
        } else if (strcmp(a, "/?") == 0 && opts) {
            out_open(1, "standard output");
            out_str(usage_text);
            out_flush();
            return EXIT_SAME;
        } else {
            if (nnames == 2)
                usage_error("too many file names", a);
            names[nnames++] = a;
        }
    }
    if (nnames < 2)
        usage_error(nnames ? "missing NEW file name" : "missing file names", NULL);

    la = copy_string(names[0]);
    lb = copy_string(names[1]);
    make_label(la, names[0]);
    make_label(lb, names[1]);

    lr_open(&ra, names[0]);
    lr_open(&rb, names[1]);

    if (outname) {
        for (i = 0; i < 2; i++) {
            same = sys_same_file(outname, names[i]);
            if (same)
                fatal(outname, same < 0 ? "can't tell whether it is an input file"
                                        : "output file is an input file", NULL);
        }
        outfd = sys_create_trunc(outname);
        if (outfd < 0)
            fatal_errno("cannot create", outname);
        out_open(outfd, outname);
    } else
        out_open(1, "standard output");

    /* Everything of a fixed size is allocated first: the keys' pages
     * then take what memory is left, and go to a temporary file when
     * there is none. */
    cmp_init();
    ba_init(&ka, sizeof(d16_u32));
    ba_init(&kb, sizeof(d16_u32));
    ch_init(&raw);
    ch_init(&ver);

    /* keys for every line */
    lr_scan(&ra, &ka);
    lr_scan(&rb, &kb);

    /* match them */
    cmp_diff(&ka, &kb, &raw);

    /* check every match, with the keys' memory free for the lists */
    {
        long na = ka.count, nb = kb.count;

        ba_free(&ka);
        ba_free(&kb);
        verify(&ra, &rb, na, nb, &raw, &ver);
        unified(&ra, &rb, na, nb, &ver, la, lb, context);
    }
    out_flush();

    if (outfd >= 0) {
        if (close(outfd) < 0) {
            int e = errno;
            outfd = -1;
            sys_unlink(outname);
            errno = e;
            fatal_errno("write error", outname);
        }
        outfd = -1;
    }
    lr_close(&ra);
    lr_close(&rb);
    temp_remove_all();
    return ch_count(&ver) ? EXIT_DIFF : EXIT_SAME;
}
