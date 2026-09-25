/* What only the instrument needs: files through the firmware's own calls,
   the engine's kernel task, and the speech through the audio engine.

   Playback hooks the eDMA channel-3 vector exactly as work/ext/isr.c, the
   payload of images 17 to 25 and at the tag pre-cleanup, proved: note
   whether the event is ours, call the original handler, re-read the
   descriptors every interrupt. Speech is added, clamped, to line 3 words 0
   and 1, the one left-right pair of the main outputs the slot probe found,
   so it mixes with the instrument instead of replacing it.

   The engine speaks at 11025 Hz into a ring, and the interrupt raises it to
   48000 by linear interpolation as it takes from the ring, so playback starts
   with the first buffer synthesised rather than the last. Running dry is
   silence until more arrives, not a jump. */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "kernel.h"
#include "device.h"

typedef int (*open_fn) (const char *path, int mode);
typedef int (*read_fn) (int h, void *buf, int len);
typedef int (*write_fn)(int h, const void *buf, int len);
typedef int (*close_fn)(int h);
#define F_OPEN  ((open_fn) 0x800DEC61u)
#define F_READ  ((read_fn) 0x800DEC49u)
#define F_WRITE ((write_fn)0x800DD9B9u)
#define F_CLOSE ((close_fn)0x800DEBD9u)

#define VEC_DMA3 ((volatile uint32_t *)0x0000004Cu)
#define EDMA_INT (*(volatile uint32_t *)0x400E8024u)

#define SAMPLES 64u
#define SLOTS   16u
#define STEP    ((uint32_t)((11025ull << 16) / 48000u))

const char *sys_log(size_t *len);
void diag_install(void);
void diag_remove(void);

void write_file(const char *path, const void *buf, size_t len)
{
    int h = F_OPEN(path, 0x601);

    if (h < 0)
        return;
    while (len > 0) {
        int chunk = len > 0x10000 ? 0x10000 : (int)len;
        if (F_WRITE(h, buf, chunk) <= 0)
            break;
        buf = (const char *)buf + chunk;
        len -= (size_t)chunk;
    }
    F_CLOSE(h);
}

const char *read_text(const char *path)
{
    static char text[4096];
    int h = F_OPEN(path, 0), n;

    if (h < 0)
        return NULL;
    n = F_READ(h, text, (int)sizeof text - 1);
    F_CLOSE(h);
    if (n <= 0)
        return NULL;
    text[n] = 0;
    return text;
}

/* One handler for the whole residency: it counts the audio engine's
   interrupts, 750 a second, which is the only trustworthy clock here, and
   plays whatever is in the ring. */
static volatile uint32_t g_ticks, g_hooked, g_probe;
static void (*orig_isr)(void);

#define RING 16384u                          /* 1.49 s at 11025 Hz */
static int16_t ring[RING];
static volatile uint32_t ring_wr;            /* samples ever written */
static volatile uint32_t ring_rd;            /* samples ever passed */
static volatile uint32_t ring_frac;          /* and the fraction into the next */
static volatile uint32_t ring_held;          /* nothing played until released */
static volatile uint32_t ring_making, ring_dry;  /* a phrase is being made; ticks it ran dry */

/* Speech level: a 16-bit sample times g_gain / 32. Set from "#vol N"
   (percent of 128) in SAY.TXT; 50, gain 64, is the level Stas approved. */
static volatile uint32_t g_gain = 64;

void target_volume(uint32_t percent)
{
    if (percent > 100)
        percent = 100;
    g_gain = percent * 128u / 100u;
    printf("volume %lu%%, gain %lu/32\n", (unsigned long)percent, (unsigned long)g_gain);
}

/* The slot probe: a quiet sine in every one of the 64 slots (four SAI lines
   of sixteen), each at its own frequency, added to what the firmware put
   there. Recording the main outputs then names every slot in one run. */
#define PROBE_SLOTS 64
#define PROBE_AMP   32768                     /* 2^19 / 16 */
static int16_t probe_sine[1024];
static uint32_t probe_phase[PROBE_SLOTS], probe_inc[PROBE_SLOTS];

static uint32_t probe_freq(uint32_t k)
{
    return 300u + 41u * k;
}

uint32_t device_ticks(void)
{
    return g_ticks;
}

static void probe_fill(void)
{
    uint32_t b, f, s;

    for (b = 0; b < 4u; b++) {
        volatile int32_t *q = (volatile int32_t *)*(volatile uint32_t *)(0x400E9000u + b * 32u);
        for (f = 0; f < SAMPLES; f++)
            for (s = 0; s < SLOTS; s++) {
                uint32_t k = b * SLOTS + s;
                q[f * SLOTS + s] += (probe_sine[probe_phase[k] >> 22] * PROBE_AMP) >> 15;
                probe_phase[k] += probe_inc[k];
            }
    }
}

static void our_isr(void)
{
    /* Read before the original handler runs: it clears the flag, so a check
       afterwards always reads zero and every interrupt looks like another
       channel's. */
    uint32_t mine = (EDMA_INT >> 3) & 1u;
    uint32_t f, s, rd, frac, wr;
    volatile int32_t *q;

    orig_isr();
    if (!mine)
        return;
    g_ticks++;
    if (g_probe) {
        probe_fill();
        return;
    }
    rd = ring_rd;
    wr = ring_wr;
    if (rd >= wr || ring_held) {
        if (rd >= wr && ring_making && !ring_held)
            ring_dry++;
        return;
    }
    /* Line 3 is the only line that reaches the main outputs, and words 0
       and 1 of its frames are one left-right pair there (measured with the
       slot probe). Speech is added to what the firmware put in them and
       clamped to the 20-bit field, so a loud mix saturates instead of
       wrapping round. */
    q = (volatile int32_t *)*(volatile uint32_t *)(0x400E9000u + 3u * 32u);
    frac = ring_frac;
    for (f = 0; f < SAMPLES && rd < wr; f++) {
        int32_t a = ring[rd % RING];
        int32_t c = rd + 1 < wr ? ring[(rd + 1) % RING] : a;
        int32_t v = a + (((c - a) * (int32_t)frac) >> 16);

        v = (v * (int32_t)g_gain) >> 5;
        for (s = 0; s < 2u; s++) {
            int32_t m = q[f * SLOTS + s] + v;
            if (m > 524287)
                m = 524287;
            else if (m < -524288)
                m = -524288;
            q[f * SLOTS + s] = m;
        }
        frac += STEP;
        rd += frac >> 16;
        frac &= 0xFFFFu;
    }
    ring_frac = frac;
    ring_rd = rd;
}

/* Whether a phrase is still being made, and how many audio interrupts found
   the ring empty meanwhile: each one is 1.3 ms of a stall. */
uint32_t audio_making(int making)
{
    uint32_t dry = ring_dry;

    ring_making = making != 0;
    if (making)
        ring_dry = 0;
    return dry;
}

/* Holds back what the ring has until it can play without running dry. */
void audio_hold(int hold)
{
    ring_held = hold != 0;
}

size_t audio_space(void)
{
    return RING - 1u - (ring_wr - ring_rd);
}

size_t audio_pending(void)
{
    return ring_wr - ring_rd;
}

/* Only the engine's task writes; the interrupt only reads what the count
   already covers. */
void audio_push(const int16_t *s, size_t n)
{
    uint32_t wr = ring_wr;
    size_t i;

    for (i = 0; i < n; i++)
        ring[(wr + i) % RING] = s[i];
    __asm__ volatile("dmb" ::: "memory");
    ring_wr = wr + (uint32_t)n;
}

/* Drops what has not been played. The interrupt moves the read side, so the
   two words change together with it held off. */
void audio_flush(void)
{
    uint32_t primask;

    __asm__ volatile("mrs %0, primask\n cpsid i" : "=r"(primask) :: "memory");
    ring_rd = ring_wr;
    ring_frac = 0;
    __asm__ volatile("msr primask, %0" :: "r"(primask) : "memory");
}

static void hook_audio(void)
{
    uint32_t installed = (uint32_t)(void *)our_isr | 1u;

    orig_isr = (void (*)(void))*VEC_DMA3;
    *VEC_DMA3 = installed;
    g_hooked = *VEC_DMA3 == installed;
}

static void unhook_audio(void)
{
    if (g_hooked)
        *VEC_DMA3 = (uint32_t)(void *)orig_isr;
    g_hooked = 0;
}

int audio_hooked(void)
{
    return g_hooked != 0;
}

void target_probe_slots(void)
{
    uint32_t k, t0;

    for (k = 0; k < 1024; k++)
        probe_sine[k] = (int16_t)(32767.0f * sinf(6.28318530718f * (float)k / 1024.0f));
    for (k = 0; k < PROBE_SLOTS; k++) {
        probe_phase[k] = 0;
        probe_inc[k] = (uint32_t)(((uint64_t)probe_freq(k) << 32) / 48000u);
        printf("slot line %lu word %2lu: %lu Hz\n", (unsigned long)(k / SLOTS),
               (unsigned long)(k % SLOTS), (unsigned long)probe_freq(k));
    }
    if (!g_hooked) {
        printf("probe: audio hook not installed\n");
        return;
    }
    t0 = g_ticks;
    g_probe = 1;
    while (g_ticks - t0 < 6u * 750u)
        target_sleep(20);
    g_probe = 0;
    printf("probe: ran %lu ticks\n", (unsigned long)(g_ticks - t0));
}

/* The engine copies itself to the eMMC, so the boot loader finds it there
   when no card is in. Only the card's file can be copied: the image in
   memory has been running, and its data is no longer what was loaded. So
   the copy happens when the eMMC's is missing or differs and the card holds
   exactly the image that is running, and it is read back and checked. A
   copy cut short by switching off fails the loader's own check, which then
   says there is no engine rather than running it. */
#define ENGINE_CARD "A:/EVV/ENGINE.BIN"
#define ENGINE_EMMC "B:/TALLFREE.BIN"

extern char __image_start[];
struct image_header {
    uint32_t magic, length, entry, crc;
};

static uint32_t crc_more(uint32_t c, const uint8_t *p, size_t n)
{
    uint32_t k;

    while (n--) {
        c ^= *p++;
        for (k = 0; k < 8; k++)
            c = (c >> 1) ^ (0xEDB88320u & -(c & 1u));
    }
    return c;
}

static int read_header(const char *path, struct image_header *h)
{
    int f = F_OPEN(path, 0), n;

    if (f < 0)
        return 0;
    n = F_READ(f, h, (int)sizeof *h);
    F_CLOSE(f);
    return n == (int)sizeof *h && h->magic == 0x32565645u;
}

static int same_image(const struct image_header *a, const struct image_header *b)
{
    return a->crc == b->crc && a->length == b->length;
}

/* The CRC of everything after the header, as mkimage.py and the loader
   compute it, over a whole file; zero if its length is wrong. */
static int file_matches(const char *path, const struct image_header *want, uint8_t *buf)
{
    int f = F_OPEN(path, 0), n;
    uint32_t total = 0, c = 0xFFFFFFFFu;

    if (f < 0)
        return 0;
    while ((n = F_READ(f, buf, 0x10000)) > 0) {
        uint32_t skip = total < sizeof *want ? sizeof *want - total : 0;
        if (skip < (uint32_t)n)
            c = crc_more(c, buf + skip, (size_t)n - skip);
        total += (uint32_t)n;
    }
    F_CLOSE(f);
    return total == want->length && ~c == want->crc;
}

void engine_install(void)
{
    const struct image_header *own = (const struct image_header *)(void *)__image_start;
    struct image_header card, emmc;
    uint8_t *buf;
    uint32_t total = 0;
    int from, to, n;

    if (read_header(ENGINE_EMMC, &emmc) && same_image(&emmc, own)) {
        printf("install: the eMMC's engine is this one\n");
        return;
    }
    if (!read_header(ENGINE_CARD, &card) || !same_image(&card, own)) {
        printf("install: the card does not hold this engine, nothing copied\n");
        return;
    }
    buf = malloc(0x10000);
    if (buf == NULL) {
        printf("install: no memory for the copy\n");
        return;
    }
    from = F_OPEN(ENGINE_CARD, 0);
    to = F_OPEN(ENGINE_EMMC, 0x601);
    if (from >= 0 && to >= 0)
        while ((n = F_READ(from, buf, 0x10000)) > 0) {
            if (F_WRITE(to, buf, n) <= 0)
                break;
            total += (uint32_t)n;
        }
    if (from >= 0)
        F_CLOSE(from);
    if (to >= 0)
        F_CLOSE(to);
    printf("install: %lu of %lu bytes to %s, %s\n", (unsigned long)total,
           (unsigned long)own->length, ENGINE_EMMC,
           file_matches(ENGINE_EMMC, own, buf) ? "read back and checked" : "CHECK FAILED");
    free(buf);
}

/* The engine runs in a task of its own at low priority, so the boot loader
   returns to the firmware at once and the interface keeps going while it
   speaks. The
   task's own stack only has to carry it into engine_run, which moves onto
   the engine's. */
#define TASK_PRIORITY 30
#define TASK_STACK    8192

static int g_sleep_sem = -1, g_task;
static int (*g_body)(void);
static uint64_t g_task_stack[TASK_STACK / 8];
static uint32_t g_kernel_crc, g_task_sp;

/* The kernel's code in ITCM, from the end of the vector table to the end of
   what the firmware loads there. A task given a stack at zero once wrote over
   it, and the next allocation anywhere in the firmware ran the damage. */
static uint32_t kernel_code_crc(void)
{
    const volatile uint8_t *p = (const volatile uint8_t *)0x400u;
    uint32_t crc = 0xFFFFFFFFu, i, k;

    for (i = 0; i < 0x1F800u; i++) {
        crc ^= p[i];
        for (k = 0; k < 8; k++)
            crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1u));
    }
    return ~crc;
}

int engine_task_id(void)
{
    return g_task;
}

/* The log so far to the card. Safe from the engine's task at any point: the
   file calls take the file system's own lock. */
void target_checkpoint(void)
{
    size_t len;
    const char *log;
    uint32_t crc = kernel_code_crc();

    printf("checkpoint at %lu ticks: task stack at %08lx, buffer %08lx to %08lx\n",
           (unsigned long)g_ticks, (unsigned long)g_task_sp,
           (unsigned long)(uintptr_t)g_task_stack,
           (unsigned long)((uintptr_t)g_task_stack + sizeof g_task_stack));
    printf("kernel code crc %08lx at launch, %08lx now, %s\n", (unsigned long)g_kernel_crc,
           (unsigned long)crc, crc == g_kernel_crc ? "unchanged" : "CHANGED");
    log = sys_log(&len);
    write_file("A:/EVV/LOG.TXT", log, len);
}

void target_done(int rc)
{
    screen_remove();
    target_sleep(50);
    printf("engine finished, code %d\n", rc);
    screen_log_write();
    target_checkpoint();
}

void target_enter(void)
{
    diag_install();
    hook_audio();
}

void target_leave(void)
{
    unhook_audio();
    diag_remove();
}

void target_sleep(int ms)
{
    if (g_sleep_sem > 0)
        kernel_sem_wait(g_sleep_sem, ms);
}

/* Cuts a sleep short. Never waits, so the screen hook may call it from
   inside the interface's drawing. */
void target_wake(void)
{
    if (g_sleep_sem > 0)
        kernel_sem_signal(g_sleep_sem);
}

/* The engine runs until the instrument is switched off. Should it ever
   return, after a fault it caught, the task gives back its semaphore, one
   of the kernel's 32, and itself. */
static void engine_task(int code, void *arg)
{
    int sem;

    (void)code;
    (void)arg;
    g_task = kernel_task_self();
    __asm__ volatile("mov %0, sp" : "=r"(g_task_sp));
    g_body();
    sem = g_sleep_sem;
    g_sleep_sem = -1;
    kernel_sem_delete(sem);
    kernel_task_exit_delete();
}

int target_launch(int (*body)(void))
{
    int task, rc;

    g_body = body;
    g_kernel_crc = kernel_code_crc();
    g_sleep_sem = kernel_sem_create("EVVsleep", 0, 1);
    if (g_sleep_sem <= 0)
        return 0x300 | (g_sleep_sem & 0xFF);
    task = kernel_task_create("EVV", engine_task, 0, TASK_PRIORITY,
                              g_task_stack, (int)sizeof g_task_stack);
    if (task <= 0)
        return 0x100 | (task & 0xFF);
    rc = kernel_task_start(task, 0);
    return rc < 0 ? (0x200 | (rc & 0xFF)) : task;
}
