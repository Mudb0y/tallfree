/* The C library's system calls, for a machine with no system.

   The heap is the gap the linker script leaves between the end of the image
   and the stacks. Output goes out through semihosting under QEMU and nowhere
   on the instrument, where a semihosting breakpoint would fault the core;
   SEMIHOST decides which at build time. */

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/times.h>

extern char __heap_start[], __heap_end[];

static char *brk = __heap_start;

void *_sbrk(ptrdiff_t n)
{
    char *old = brk;

    if (n < 0 || brk + n > __heap_end) {
        errno = ENOMEM;
        return (void *)-1;
    }
    brk += n;
    return old;
}

/* How far the heap got, for reporting what a run cost. */
uint32_t sys_heap_used(void)
{
    return (uint32_t)(brk - __heap_start);
}

#ifdef SEMIHOST

static int semihost(int op, void *arg)
{
    register int r0 __asm__("r0") = op;
    register void *r1 __asm__("r1") = arg;

    __asm__ volatile("bkpt 0xab" : "+r"(r0) : "r"(r1) : "memory");
    return r0;
}

int sh_open(const char *path, int mode)
{
    uint32_t a[3] = { (uint32_t)path, (uint32_t)mode, (uint32_t)strlen(path) };
    return semihost(0x01, a);
}

int sh_write(int fd, const void *buf, int len)
{
    uint32_t a[3] = { (uint32_t)fd, (uint32_t)buf, (uint32_t)len };
    return len - semihost(0x05, a);
}

int sh_close(int fd)
{
    uint32_t a[1] = { (uint32_t)fd };
    return semihost(0x02, a);
}

int sh_read(int fd, void *buf, int len)
{
    uint32_t a[3] = { (uint32_t)fd, (uint32_t)buf, (uint32_t)len };
    return len - semihost(0x06, a);
}

int sh_flen(int fd)
{
    uint32_t a[1] = { (uint32_t)fd };
    return semihost(0x0C, a);
}

__attribute__((noreturn)) void sh_exit(int code)
{
    uint32_t a[2] = { 0x20026u, (uint32_t)code };
    semihost(0x20, a);
    for (;;)
        ;
}

static int console = -1;

int _write(int fd, const char *buf, int len)
{
    (void)fd;
    if (console < 0)
        console = sh_open(":tt", 4);
    return sh_write(console, buf, len);
}

__attribute__((noreturn)) void _exit(int code)
{
    sh_exit(code);
}

#else

/* On the instrument there is no console, so output collects here and the
   target writes it to the card at the end. */
static char log_buf[16384];
static size_t log_len;

int _write(int fd, const char *buf, int len)
{
    (void)fd;
    if (len > (int)(sizeof log_buf - log_len))
        len = (int)(sizeof log_buf - log_len);
    memcpy(log_buf + log_len, buf, (size_t)len);
    log_len += (size_t)len;
    return len;
}

const char *sys_log(size_t *len)
{
    fflush(stdout);
    *len = log_len;
    return log_buf;
}

/* Nothing on the instrument may hang the task that called in, so an exit
   goes back to engine_main with its code. */
__attribute__((noreturn)) void engine_exit(int code);

__attribute__((noreturn)) void _exit(int code)
{
    engine_exit(code);
}

#endif

int _read(int fd, char *buf, int len)
{
    (void)fd;
    (void)buf;
    (void)len;
    return 0;
}

int _open(const char *path, int flags, int mode)
{
    (void)path;
    (void)flags;
    (void)mode;
    errno = ENOENT;
    return -1;
}

int _close(int fd)
{
    (void)fd;
    return -1;
}

int _lseek(int fd, int off, int whence)
{
    (void)fd;
    (void)off;
    (void)whence;
    return 0;
}

int _fstat(int fd, struct stat *st)
{
    (void)fd;
    memset(st, 0, sizeof *st);
    st->st_mode = S_IFCHR;
    return 0;
}

int _stat(const char *path, struct stat *st)
{
    (void)path;
    (void)st;
    errno = ENOENT;
    return -1;
}

int _isatty(int fd)
{
    (void)fd;
    return 1;
}

int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

int _getpid(void)
{
    return 1;
}

clock_t _times(struct tms *t)
{
    memset(t, 0, sizeof *t);
    return (clock_t)-1;
}

int _gettimeofday(void *tv, void *tz)
{
    (void)tv;
    (void)tz;
    return -1;
}

int _unlink(const char *path)
{
    (void)path;
    errno = ENOENT;
    return -1;
}

int _link(const char *a, const char *b)
{
    (void)a;
    (void)b;
    errno = EMLINK;
    return -1;
}

ssize_t readlink(const char *path, char *buf, size_t len)
{
    (void)path;
    (void)buf;
    (void)len;
    errno = ENOENT;
    return -1;
}
