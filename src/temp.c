/* temp.c - temporary files (docs/SPEC.md, "Temporary files")
 *
 * A temporary file is made in the directory named by TMP, else TEMP,
 * else the current directory: the first where one can be created. Its
 * name is D16xxxxx.TMP, five hex digits from the timer, created
 * exclusively so that an existing file is never reused. Every temporary
 * file is closed and removed before diff16 exits, on errors too.
 */

#include "d16.h"

#define TEMP_MAX   8
#define TEMP_TRIES 32

#ifdef __WATCOMC__
#define SEP '\\'
#else
#define SEP '/'
#endif

static int tfd[TEMP_MAX];
static char tpath[TEMP_MAX][SYS_PATH_MAX];
static int ntemp;
static unsigned serial;

/* Make dir + name in path; 0 if it doesn't fit. */
static int make_name(char *path, const char *dir, unsigned long n)
{
    static const char hex[] = "0123456789ABCDEF";
    unsigned len = (unsigned)strlen(dir);
    int i;

    if (len + 14 > SYS_PATH_MAX)
        return 0;
    memcpy(path, dir, len);
    if (len && dir[len - 1] != '\\' && dir[len - 1] != '/' && dir[len - 1] != ':')
        path[len++] = SEP;
    memcpy(path + len, "D16", 3);
    len += 3;
    for (i = 4; i >= 0; i--)
        path[len++] = hex[(unsigned)(n >> (4 * i)) & 15];
    memcpy(path + len, ".TMP", 5);
    return 1;
}

int temp_create(void)
{
    static const char *const vars[] = { "TMP", "TEMP", NULL };
    const char *dirs[3];
    char *path;
    int nd = 0, d, t, fd, err = ENOENT;
    unsigned long n;

    if (ntemp == TEMP_MAX)
        fatal("too many temporary files", NULL, NULL);
    for (d = 0; vars[d]; d++) {
        const char *v = sys_getenv(vars[d]);
        if (v && *v)
            dirs[nd++] = v;
    }
    dirs[nd++] = "";

    path = tpath[ntemp];
    for (d = 0; d < nd; d++) {
        for (t = 0; t < TEMP_TRIES; t++) {
            n = ((unsigned long)sys_ticks() * 31 + serial++ * 7919UL) & 0xFFFFFUL;
            if (!make_name(path, dirs[d], n)) {
                err = ENOENT;
                break;
            }
            fd = sys_create_new(path);
            if (fd >= 0) {
                tfd[ntemp++] = fd;
                return fd;
            }
            err = errno;
            if (err != EEXIST)
                break;
        }
    }
    errno = err;
    fatal_errno("cannot create temporary file", path[0] ? path : "D16xxxxx.TMP");
    return -1;
}

void temp_remove_all(void)
{
    while (ntemp) {
        ntemp--;
        close(tfd[ntemp]);
        sys_unlink(tpath[ntemp]);
    }
}
