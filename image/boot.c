/* Starts the OpenEVV engine when the instrument boots.

   The image points the main screen's status-line draw, the vtable word at
   0x80226E98, at boot_hook here in flash. The first time the main screen
   draws, which is after start-up, boot_hook puts the word back, so it never
   runs again, and then loads the engine: from A:/EVV/ENGINE.BIN on the
   card, or failing that from B:/TALLFREE.BIN on the eMMC, where the engine
   copies itself. A card holding A:/EVV/NOENGINE loads nothing, which is the
   way back from an engine that stops the unit starting. With no engine
   anywhere, a clip in flash says so, through the same audio interrupt the
   engine uses, without holding up the start.

   Nothing is jumped into unless the header checks out: magic, a length that
   matches what was read, an entry point inside the image, and a CRC over
   everything after the header. The image arrives by whatever path the card
   driver takes, so the region is cleaned and invalidated from the D-cache
   before the read, cleaned after it, and the I-cache is invalidated before
   the jump.

   This runs from flash, which is not writable, so it keeps no state of its
   own; the clip's playing position lives at the start of the engine's
   region, which is free when there is no engine.

   It writes nothing to the card: a write from inside the interface's first
   draw hung the unit on a card whose FAT was inconsistent. Where the engine
   came from, and why the card's copy was refused if it was, go to the engine
   as its arguments instead, for its own log. */

/* The region images 30 and 31 give the engine. Its engines carry "EVV2"; an
   image 29 engine, "EVV1", linked 1 MB higher, is refused, not run at the
   wrong address. */
#define ENGINE_BASE 0x83AC0000u
#define ENGINE_MAX  0x00530000u
#define MAGIC       0x32565645u              /* "EVV2" */

typedef int (*open_fn) (const char *path, int mode);
typedef int (*read_fn) (int h, void *buf, int len);
typedef int (*close_fn)(int h);
#define F_OPEN  ((open_fn) 0x800DEC61u)
#define F_READ  ((read_fn) 0x800DEC49u)
#define F_CLOSE ((close_fn)0x800DEBD9u)

/* The engine's first argument: this mark with 1 for the card or 2 for the
   eMMC, so an engine started by an older loader, which passes nothing, can
   tell. The second is why the card's copy was not used, as load answers. */
#define FROM_MARK   0x4C4F0000u

#define DCCIMVAC (*(volatile unsigned int *)0xE000EF70u)
#define DCCMVAC  (*(volatile unsigned int *)0xE000EF68u)
#define ICIALLU  (*(volatile unsigned int *)0xE000EF50u)

#define STATUS_WORD (*(volatile unsigned int *)0x80226E98u)
#define STATUS_DRAW 0x8014A019u

#define VEC_DMA3 (*(volatile unsigned int *)0x0000004Cu)
#define EDMA_INT (*(volatile unsigned int *)0x400E8024u)

struct header {
    unsigned int magic;
    unsigned int length;         /* bytes of image, header included */
    unsigned int entry;          /* thumb address of engine_start */
    unsigned int crc;            /* CRC-32 of everything after the header */
};

void boot_main(void);

/* The draw call's registers and stack go through untouched: eight words
   keep the C call aligned, and the real draw is jumped to, not called. */
__asm__(
".syntax unified\n"
".thumb\n"
".section .text.boot_hook,\"ax\",%progbits\n"
".globl boot_hook\n"
".thumb_func\n"
".type boot_hook, %function\n"
"boot_hook:\n"
"    push  {r0-r5, r12, lr}\n"
"    bl    boot_main\n"
"    pop   {r0-r5, r12, lr}\n"
"    ldr   pc, =0x8014A019\n"
".ltorg\n"
);

/* The clip, 16-bit mono at 11025 Hz, rendered by the desktop engine. */
__asm__(
".section .rodata.clip,\"a\",%progbits\n"
".balign 4\n"
".globl clip, clip_end\n"
"clip:\n"
".incbin \"noengine.raw\"\n"
"clip_end:\n"
);
extern const short clip[], clip_end[];

static void dcache_range(volatile unsigned int *op, unsigned int start, unsigned int len)
{
    unsigned int a;

    for (a = start & ~31u; a < start + len; a += 32)
        *op = a;
    __asm__ volatile("dsb" ::: "memory");
}

static unsigned int crc32(const unsigned char *p, unsigned int n)
{
    unsigned int c = 0xFFFFFFFFu, k;

    while (n--) {
        c ^= *p++;
        for (k = 0; k < 8; k++)
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

static int exists(const char *path)
{
    int h = F_OPEN(path, 0);

    if (h < 0)
        return 0;
    F_CLOSE(h);
    return 1;
}

/* Reads an engine image into its place and answers 0 if it can run, or why
   not: 1 no such file, 2 not an engine image, 3 truncated, 4 its entry
   outside it, 5 a CRC that does not match. */
static unsigned int load(const char *path)
{
    struct header *hd = (struct header *)ENGINE_BASE;
    unsigned char *dst = (unsigned char *)ENGINE_BASE;
    unsigned int total = 0;
    int h, n;

    h = F_OPEN(path, 0);
    if (h < 0)
        return 1;
    dcache_range(&DCCIMVAC, ENGINE_BASE, ENGINE_MAX);
    for (;;) {
        unsigned int want = ENGINE_MAX - total;
        if (want == 0)
            break;
        if (want > 0x10000u)
            want = 0x10000u;
        n = F_READ(h, dst + total, (int)want);
        if (n <= 0)
            break;
        total += (unsigned int)n;
    }
    F_CLOSE(h);

    if (total < sizeof *hd || hd->magic != MAGIC)
        return 2;
    if (hd->length != total)
        return 3;
    if (hd->entry < ENGINE_BASE || hd->entry >= ENGINE_BASE + total)
        return 4;
    if (crc32(dst + sizeof *hd, total - sizeof *hd) != hd->crc)
        return 5;
    dcache_range(&DCCMVAC, ENGINE_BASE, total);
    ICIALLU = 0;
    __asm__ volatile("dsb\n isb" ::: "memory");
    return 0;
}

/* The clip's player. Its state sits where the engine would have been. */
struct player {
    unsigned int pos, frac;
    void (*orig)(void);
};
#define PLAYER ((volatile struct player *)ENGINE_BASE)
#define STEP   ((unsigned int)((11025ull << 16) / 48000u))

/* As the engine's own interrupt: note whether the event is ours before the
   firmware's handler clears it, add the clip at half level to line 3 words
   0 and 1, the main outputs' left-right pair, and clamp to the 20-bit
   field. At the end, the firmware gets its vector back. */
static void clip_isr(void)
{
    unsigned int mine = (EDMA_INT >> 3) & 1u;
    unsigned int n = (unsigned int)(clip_end - clip), pos, frac, f, s;
    volatile int *q;

    PLAYER->orig();
    if (!mine)
        return;
    pos = PLAYER->pos;
    frac = PLAYER->frac;
    if (pos + 1 >= n) {
        VEC_DMA3 = (unsigned int)PLAYER->orig;
        return;
    }
    q = (volatile int *)*(volatile unsigned int *)(0x400E9000u + 3u * 32u);
    for (f = 0; f < 64u && pos + 1 < n; f++) {
        int a = clip[pos], c = clip[pos + 1];
        int v = (a + (((c - a) * (int)frac) >> 16)) * 2;
        for (s = 0; s < 2u; s++) {
            int m = q[f * 16u + s] + v;
            if (m > 524287)
                m = 524287;
            else if (m < -524288)
                m = -524288;
            q[f * 16u + s] = m;
        }
        frac += STEP;
        pos += frac >> 16;
        frac &= 0xFFFFu;
    }
    PLAYER->pos = pos;
    PLAYER->frac = frac;
}

static void say_no_engine(void)
{
    PLAYER->pos = 0;
    PLAYER->frac = 0;
    PLAYER->orig = (void (*)(void))VEC_DMA3;
    __asm__ volatile("dsb" ::: "memory");
    VEC_DMA3 = (unsigned int)clip_isr | 1u;
}

void boot_main(void)
{
    unsigned int card, from;

    STATUS_WORD = STATUS_DRAW;
    __asm__ volatile("dsb" ::: "memory");
    if (exists("A:/EVV/NOENGINE"))
        return;
    if ((card = load("A:/EVV/ENGINE.BIN")) == 0)
        from = 1;
    else if (load("B:/TALLFREE.BIN") == 0)
        from = 2;
    else {
        say_no_engine();
        return;
    }
    ((int (*)(unsigned int, unsigned int))((struct header *)ENGINE_BASE)->entry)(FROM_MARK | from,
                                                                              card);
}
