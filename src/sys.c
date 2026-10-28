/* sys.c - diff16's platform layer: DOS through libc16, or a POSIX host */

#include "d16.h"

static volatile int broke;

#ifdef __WATCOMC__

#include <dos.h>
#include <sys/stat.h>

/* INT 23h, Ctrl-C or Ctrl-Break: note it and let DOS carry on, so that
 * the program stops at its next check and removes its temporary files. */
static void __interrupt __far on_break(void)
{
    broke = 1;
}

void sys_init(void)
{
    setmode(0, O_BINARY);
    setmode(1, O_BINARY);
    _dos_setvect(0x23, on_break);
}

int sys_open_read(const char *path)
{
    return open(path, O_RDONLY | O_BINARY);
}

int sys_create_new(const char *path)
{
    return open(path, O_RDWR | O_CREAT | O_EXCL | O_BINARY, S_IREAD | S_IWRITE);
}

int sys_create_trunc(const char *path)
{
    return open(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, S_IREAD | S_IWRITE);
}

/* INT 21h 41h: delete a file */
int sys_unlink(const char *path)
{
    union REGS r;
    struct SREGS s;
    const char *p = path;

    segread(&s);
    r.h.ah = 0x41;
    s.ds = FP_SEG(p);
    r.x.dx = FP_OFF(p);
    intdosx(&r, &r, &s);
    return r.x.cflag ? -1 : 0;
}

/* INT 21h 60h (DOS 3.0+): the canonical name of path in buf (128 bytes),
 * with SUBST, JOIN and ASSIGN resolved and names made upper case 8.3.
 * Falls back to _fullpath. */
static int truename(const char *path, char *buf)
{
    union REGS r;
    struct SREGS s;
    const char *p = path;
    char *q = buf;

    segread(&s);
    r.h.ah = 0x60;
    s.ds = FP_SEG(p);
    r.x.si = FP_OFF(p);
    s.es = FP_SEG(q);
    r.x.di = FP_OFF(q);
    intdosx(&r, &r, &s);
    if (!r.x.cflag)
        return 0;
    return _fullpath(buf, path, SYS_PATH_MAX) ? 0 : -1;
}

/* 1 if a and b name the same file, 0 if not, -1 if that can't be told */
int sys_same_file(const char *a, const char *b)
{
    static char fa[SYS_PATH_MAX], fb[SYS_PATH_MAX];

    if (truename(a, fa) || truename(b, fb))
        return -1;
    return stricmp(fa, fb) == 0;
}

/* The BIOS timer tick count, 18.2 a second */
unsigned sys_ticks(void)
{
    return *(unsigned __far *)MK_FP(0x40, 0x6C);
}

const char *sys_getenv(const char *name)
{
    return getenv(name);
}

#else /* a POSIX host */

#include <signal.h>
#include <sys/stat.h>
#include <time.h>

static void on_break(int sig)
{
    (void)sig;
    broke = 1;
}

void sys_init(void)
{
    signal(SIGINT, on_break);
}

int sys_open_read(const char *path)
{
    return open(path, O_RDONLY);
}

int sys_create_new(const char *path)
{
    return open(path, O_RDWR | O_CREAT | O_EXCL, 0600);
}

int sys_create_trunc(const char *path)
{
    return open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
}

int sys_unlink(const char *path)
{
    return unlink(path);
}

int sys_same_file(const char *a, const char *b)
{
    struct stat sa, sb;

    if (stat(a, &sa) || stat(b, &sb))
        return 0;           /* one doesn't exist, so they differ */
    return sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
}

unsigned sys_ticks(void)
{
    return (unsigned)time(NULL) ^ ((unsigned)getpid() << 4);
}

const char *sys_getenv(const char *name)
{
    return getenv(name);
}

#endif

int sys_interrupted(void)
{
    return broke;
}

void sys_check_break(void)
{
    if (broke)
        fatal("interrupted", NULL, NULL);
}
