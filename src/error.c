/* error.c - diagnostics and the trouble exit */

#include "d16.h"

static int dying;

/* "diff16: a: b: c", leaving out null parts */
void err_msg(const char *a, const char *b, const char *c)
{
    err_str("diff16: ");
    err_str(a);
    if (b) {
        err_str(": ");
        err_str(b);
    }
    if (c) {
        err_str(": ");
        err_str(c);
    }
    err_str("\n");
}

void fatal(const char *a, const char *b, const char *c)
{
    err_msg(a, b, c);
    if (!dying) {
        dying = 1;
        main_cleanup();
        temp_remove_all();
    }
    exit(EXIT_TROUBLE);
}

/* what failed, on which file, and why, from errno */
void fatal_errno(const char *what, const char *name)
{
    int e = errno;

    fatal(name, what, e == ENOSPC ? "disk full" : strerror(e));
}

void fatal_nomem(void)
{
    fatal("out of memory", NULL, NULL);
}
