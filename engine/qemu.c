/* What only the QEMU run needs: a vector table at address zero, a reset
   handler that turns the floating point unit on before any C runs, and
   somewhere for the samples to go. */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

int  engine_start(void);
int  sh_open(const char *path, int mode);
int  sh_write(int fd, const void *buf, int len);
int  sh_close(int fd);
int  sh_read(int fd, void *buf, int len);
int  sh_flen(int fd);
__attribute__((noreturn)) void sh_exit(int code);

void qemu_reset(void);

__attribute__((naked, noreturn)) void qemu_reset(void)
{
    __asm__ volatile(
        "ldr r0, =0xE000ED88\n"     /* CPACR: full access to CP10 and CP11 */
        "ldr r1, [r0]\n"
        "orr r1, r1, #(0xF << 20)\n"
        "str r1, [r0]\n"
        "dsb\n"
        "isb\n"
        "bl  qemu_main\n"
        "b   .\n"
        ".ltorg\n");
}

extern char __image_end[], __stack_top[];

/* The instrument hands the engine memory full of whatever was there before;
   QEMU's starts as zeroes, which hides anything that relies on that. So fill
   everything past the image with junk first, the stack included. */
__attribute__((used)) static void qemu_main(void)
{
    uint32_t *p = (uint32_t *)(((uintptr_t)__image_end + 3) & ~3u);
    uint32_t *end = (uint32_t *)((uintptr_t)__stack_top - 256);

    while (p < end)
        *p++ = 0xA5A5A5A5u;
    sh_exit(engine_start());
}

static void fault(void)
{
    static const char msg[] = "engine: fault\n";
    int fd = sh_open(":tt", 4);
    sh_write(fd, msg, sizeof msg - 1);
    sh_exit(4);
}

__attribute__((section(".qemu_vectors"), used))
static const uintptr_t vectors[16] = {
    0x20400000u,                    /* top of SSRAM2/3, until the engine moves off it */
    (uintptr_t)qemu_reset,
    (uintptr_t)fault, (uintptr_t)fault, (uintptr_t)fault,
    (uintptr_t)fault, (uintptr_t)fault,
};

void target_output(const short *samples, size_t n)
{
    int fd = sh_open("out.raw", 4 | 1);   /* "wb" */
    if (fd < 0)
        return;
    sh_write(fd, samples, (int)(n * sizeof *samples));
    sh_close(fd);
}

/* The text to speak, from in.txt beside the run if there is one. */
const char *target_input(void)
{
    int fd = sh_open("in.txt", 0), n;
    char *t;

    if (fd < 0)
        return NULL;
    n = sh_flen(fd);
    t = malloc((size_t)n + 1);
    if (t == NULL || sh_read(fd, t, n) != n) {
        sh_close(fd);
        return NULL;
    }
    sh_close(fd);
    t[n] = 0;
    return t;
}

void target_done(int rc)
{
    (void)rc;
}

void target_stage(const char *what) { (void)what; }
void target_enter(void) { }
void target_leave(void) { }

void target_probe_slots(void) { }
void target_volume(uint32_t percent) { (void)percent; }

int target_launch(int (*body)(void))
{
    return body();
}
