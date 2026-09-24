/* What only the instrument needs: the text from the card, the samples back
   to the card, the log, and the speech through the audio engine.

   Files go through the firmware's own file calls. Playback hooks the eDMA
   channel-3 vector exactly as work/ext/isr.c proved: note whether the event
   is ours, call the original handler, re-read the four descriptors every
   interrupt, write 64 samples into all 16 slots of each frame, below 2^19.
   The engine speaks at 11025 Hz and the interrupt raises it to 48000 by
   linear interpolation as it goes, so no upsampled copy is ever held.

   engine_main waits for playback to finish and puts the vector back before
   returning. The handler lives in the engine image, and the next press loads
   a fresh image over it. */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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
void target_stage(const char *what);

static void (*orig_isr)(void);
static const int16_t *g_pcm;
static volatile uint32_t g_n, g_pos, g_irqs;

static void write_file(const char *path, const void *buf, size_t len)
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

const char *target_input(void)
{
    static char text[4096];
    int h = F_OPEN("A:/EVV/SAY.TXT", 0), n;

    if (h < 0)
        return NULL;
    n = F_READ(h, text, (int)sizeof text - 1);
    F_CLOSE(h);
    if (n <= 0)
        return NULL;
    text[n] = 0;
    return text;
}

/* One handler for the whole run: it counts the audio engine's interrupts,
   750 a second, which is the only trustworthy clock here, and while g_n is
   set it also writes the speech. */
static volatile uint32_t g_ticks, g_playing, g_hooked;

uint32_t device_ticks(void)
{
    return g_ticks;
}

static void our_isr(void)
{
    uint32_t b, f, s;
    /* Read before the original handler runs: it clears the flag, so a check
       afterwards always reads zero and every interrupt looks like another
       channel's. */
    uint32_t mine = (EDMA_INT >> 3) & 1u;

    orig_isr();
    if (!mine)
        return;
    g_ticks++;
    if (!g_playing)
        return;
    g_irqs++;
    if ((g_pos >> 16) >= g_n)
        return;
    for (b = 0; b < 4u; b++) {
        volatile int32_t *q = (volatile int32_t *)*(volatile uint32_t *)(0x400E9000u + b * 32u);
        uint32_t pos = g_pos;
        for (f = 0; f < SAMPLES; f++) {
            uint32_t i = pos >> 16, frac = pos & 0xFFFFu;
            int32_t v = 0;
            if (i < g_n) {
                int32_t a = g_pcm[i];
                int32_t c = i + 1 < g_n ? g_pcm[i + 1] : 0;
                v = a + (((c - a) * (int32_t)frac) >> 16);
            }
            v <<= 3;
            for (s = 0; s < SLOTS; s++)
                q[f * SLOTS + s] = v;
            pos += STEP;
        }
    }
    g_pos += STEP * SAMPLES;
}

static void context_report(const char *when);

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

static void play(const int16_t *pcm, size_t n)
{
    uint32_t guard, t0;

    if (n == 0 || !g_hooked) {
        printf("play: nothing to do, %u samples, hooked %u\n", (unsigned)n, (unsigned)g_hooked);
        return;
    }
    g_pcm = pcm;
    g_pos = 0;
    g_irqs = 0;
    g_n = (uint32_t)n;
    t0 = g_ticks;
    g_playing = 1;
    /* Bounded by interrupts, 750 a second: the clip's worth plus a second.
       The loop count only catches an interrupt that never comes at all. */
    for (guard = 0; (g_pos >> 16) < g_n; guard++) {
        if (g_irqs > (uint32_t)((uint64_t)n * 750u / 11025u) + 750u)
            break;
        if (g_irqs == 0 && guard > 300000000u)
            break;
    }
    g_playing = 0;
    context_report("after play");
    printf("play: %u samples, %u interrupts, reached sample %u, %u ticks, loop %u\n",
           (unsigned)n, (unsigned)g_irqs, (unsigned)(g_pos >> 16),
           (unsigned)(g_ticks - t0), (unsigned)guard);
}

void target_output(const short *samples, size_t n)
{
    write_file("A:/EVV/OUT.RAW", samples, n * sizeof *samples);
    target_stage("OUT.RAW written, playing");
    play(samples, n);
}

void target_done(int rc)
{
    size_t len;
    const char *log = sys_log(&len);

    (void)rc;
    write_file("A:/EVV/LOG.TXT", log, len);
}

void diag_install(void);
void diag_remove(void);

/* What the processor says about the context the engine was entered in, and
   whether the audio interrupt can reach us from it. Named registers only:
   the peripheral windows are sparse and a sweep has faulted this core. */
static void context_report(const char *when)
{
    uint32_t ipsr, primask, basepri, faultmask, control, sa, sb;
    volatile uint32_t spin;

    __asm__ volatile("mrs %0, ipsr" : "=r"(ipsr));
    __asm__ volatile("mrs %0, primask" : "=r"(primask));
    __asm__ volatile("mrs %0, basepri" : "=r"(basepri));
    __asm__ volatile("mrs %0, faultmask" : "=r"(faultmask));
    __asm__ volatile("mrs %0, control" : "=r"(control));
    sa = *(volatile uint32_t *)0x400E9000u;
    for (spin = 0; spin < 2000000u; spin++)
        ;
    sb = *(volatile uint32_t *)0x400E9000u;
    printf("%s: ipsr %lx primask %lx basepri %lx faultmask %lx control %lx\n",
           when, (unsigned long)ipsr, (unsigned long)primask, (unsigned long)basepri,
           (unsigned long)faultmask, (unsigned long)control);
    printf("%s: vtor %lx iser0 %lx ispr0 %lx iabr0 %lx ipr3 %lx vec19 %lx\n", when,
           (unsigned long)*(volatile uint32_t *)0xE000ED08u,
           (unsigned long)*(volatile uint32_t *)0xE000E100u,
           (unsigned long)*(volatile uint32_t *)0xE000E200u,
           (unsigned long)*(volatile uint32_t *)0xE000E300u,
           (unsigned long)*(volatile uint8_t *)0xE000E403u,
           (unsigned long)*VEC_DMA3);
    printf("%s: edma erq %lx int %lx, tcd0 saddr %lx then %lx, ticks %lu\n", when,
           (unsigned long)*(volatile uint32_t *)0x400E800Cu,
           (unsigned long)EDMA_INT, (unsigned long)sa, (unsigned long)sb,
           (unsigned long)g_ticks);
}

void target_enter(void)
{
    diag_install();
    hook_audio();
    target_stage("engine entered");
    context_report("entry");
}

void target_leave(void)
{
    unhook_audio();
    diag_remove();
}
