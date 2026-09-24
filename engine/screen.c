/* What the screen draws, as a screen reader would speak it.

   Every string on the normal screens goes through one DrawString,
   FUN_800EE530(surface, x, y, string, length), and every clear through one
   of two calls, the whole surface (vtable slot 0x08) or a rectangle (slot
   0xC0). All three are reached only through seven surface vtables, so
   pointing those words at our hooks is a data write: no instruction changes
   and the caches need nothing. The hooks run inside the interface's own
   drawing, so they only copy the call into a ring and wake the engine's
   task; they never wait, allocate or touch the card. Then each jumps on to
   the real function with every register and the stack as they came.

   The engine's task drains the ring into a model of the screen, one item
   per surface and position, with what was erased and not drawn again taken
   off it. When the screen settles it decides what is worth saying:
   entering a screen, its title and the focused item; moving the focus, the
   new item; turning a value, the value, with its label the first time; a
   pop-up, its text. The rest stays silent. */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "kernel.h"
#include "device.h"

#define SLOT_CLEAR 0x08u
#define SLOT_FILL  0xC0u
#define SLOT_MARK  0x18u
#define SLOT_TEXT  0x12Cu

#ifndef SIM
#define DRAW_STRING     0x800EE531u
#define DRAW_STRING_ASM "0x800EE531"
#define CLEAR_ALL       0x800F14D1u
#define CLEAR_ALL_ASM   "0x800F14D1"
#define FILL_RECT       0x800EDEE9u
#define FILL_RECT_ASM   "0x800EDEE9"
static const uint32_t vt_base[] = {
    0x8021D1A4u, 0x8021D340u, 0x8021D4DCu, 0x8021D678u,
    0x8021E2A4u, 0x8021E438u, 0x8021E5D4u,
};
#else
/* The simulator's stand-ins, from sim.c. */
extern volatile uint32_t sim_vtables[7][0x130 / 4], sim_site;
void sim_draw_string(void);
void sim_clear(void);
void sim_fill(void);
void sim_batch(const char *phrases, int count);
#define DRAW_STRING     ((uint32_t)(uintptr_t)sim_draw_string)
#define DRAW_STRING_ASM "sim_draw_string"
#define CLEAR_ALL       ((uint32_t)(uintptr_t)sim_clear)
#define CLEAR_ALL_ASM   "sim_clear"
#define FILL_RECT       ((uint32_t)(uintptr_t)sim_fill)
#define FILL_RECT_ASM   "sim_fill"
static const uint32_t vt_base[] = {
    (uint32_t)(uintptr_t)sim_vtables[0], (uint32_t)(uintptr_t)sim_vtables[1],
    (uint32_t)(uintptr_t)sim_vtables[2], (uint32_t)(uintptr_t)sim_vtables[3],
    (uint32_t)(uintptr_t)sim_vtables[4], (uint32_t)(uintptr_t)sim_vtables[5],
    (uint32_t)(uintptr_t)sim_vtables[6],
};
#endif
#define VTABLES   (sizeof vt_base / sizeof vt_base[0])
#define VT(i, slot) (*(volatile uint32_t *)(vt_base[i] + (slot)))

/* Where DrawString's callers keep their own return address: the three text
   wrappers push a fixed frame, so it sits a fixed number of words above the
   stacked length. The title setter FUN_800C2558 draws every page title
   through the first of them, returning to 0x80081011. */
#define WRAP_TEXT    0x800D810Fu            /* FUN_800D80E0, five words up */
#define WRAP_CENTRE  0x800D5005u            /* FUN_800D4FA0, thirteen */
#define WRAP_PLAIN   0x800EE06Fu            /* FUN_800EE048, seven */
#define TITLE_SITE   0x80081011u

/* What particular drawing code is for, by the return address the hook reads
   one level up, from what the unit's logs recorded. Anything else is judged
   by how it is drawn. */
enum { ROLE_NONE, ROLE_TITLE, ROLE_TOAST, ROLE_PAD, ROLE_MAIN, ROLE_IGNORE, ROLE_TABS };
static const struct { uint32_t site; uint8_t role; } sites[] = {
    { TITLE_SITE,  ROLE_TITLE },            /* the page title setter */
    { 0x80151801u, ROLE_TITLE },            /* EXPORT SAMPLE/PROJ./MULTIPAD */
    { 0x801120A1u, ROLE_TITLE },            /* CHROMATIC MODE */
    { 0x80146135u, ROLE_TITLE },            /* an effect's name, over its grid or page */
    { 0x80195269u, ROLE_TITLE },            /* 16 VELOCITY, PAD LINK GROUPS, MUTE GROUP */
    { 0x8013FE3Du, ROLE_IGNORE },           /* a second copy of those, a pixel over */
    { 0x8006BECDu, ROLE_TOAST },            /* STOP, RECORDING, METRO MODE ON */
    { 0x80149DD5u, ROLE_PAD },              /* the main screen's bank and pad */
    { 0x80149DC7u, ROLE_MAIN },             /* its pattern */
    { 0x80149DE5u, ROLE_MAIN },             /* the pad's bus, DRY or BUS-1 */
    { 0x80149DF5u, ROLE_MAIN },             /* fixed velocity, Fix or Vel */
    { 0x80121751u, ROLE_TABS },             /* a page's tabs, GENERAL CLICK MIDI */
    { 0x8017388Bu, ROLE_IGNORE },           /* LEVEL meters */
    { 0x801779C9u, ROLE_IGNORE },
};

static uint8_t role_of(uint32_t site)
{
    unsigned i;

    for (i = 0; i < sizeof sites / sizeof sites[0]; i++)
        if (sites[i].site == site)
            return sites[i].role;
    return ROLE_NONE;
}

static volatile int unload_flag, screen_hooked, screen_mode;

/* The engine runs from boot until the instrument is switched off; nothing
   on the unit asks it to stop. The simulator does, to end a run. */
void request_unload(void)
{
    unload_flag = 1;
    target_wake();
}

int unload_requested(void)
{
    return unload_flag;
}

/* One call as a hook saw it. */
enum { EV_TEXT, EV_CLEAR, EV_FILL };
#define DRAW_TEXT 64
struct draw {
    uint32_t tick, surf, lr, site;
    int32_t  mark;
    int16_t  x, y, x1, y1;
    uint8_t  kind, task, len;
    char     text[DRAW_TEXT];
};

#define DRAWS 1024
static struct draw draws[DRAWS];
static volatile uint32_t draw_wr, draw_rd, draw_lost, draw_seen;

void text_hook(void);
void clear_hook(void);
void fill_hook(void);
/* Eight words keep the stack 8-byte aligned for the C call, and leave the
   caller's fifth argument at sp + 32. */
#define HOOK(name, record, target)                                        \
    __asm__(".syntax unified\n.thumb\n.text\n"                            \
            ".globl " #name "\n.thumb_func\n.type " #name ", %function\n" \
            #name ":\n"                                                   \
            "    push  {r0-r5, r12, lr}\n"                                \
            "    mov   r0, sp\n"                                          \
            "    bl    " #record "\n"                                     \
            "    pop   {r0-r5, r12, lr}\n"                                \
            "    ldr   pc, =" target "\n"                                 \
            ".ltorg\n")
HOOK(text_hook, text_record, DRAW_STRING_ASM);
HOOK(clear_hook, clear_record, CLEAR_ALL_ASM);
HOOK(fill_hook, fill_record, FILL_RECT_ASM);

static void push(const struct draw *d)
{
    uint32_t primask, w;

    __asm__ volatile("mrs %0, primask\n cpsid i" : "=r"(primask) :: "memory");
    w = draw_wr;
    if (w - draw_rd >= DRAWS) {
        draw_lost++;
    } else {
        memcpy(&draws[w % DRAWS], d, sizeof *d);
        draw_wr = w + 1;
    }
    __asm__ volatile("msr primask, %0" :: "r"(primask) : "memory");
    target_wake();
}

/* f holds r0 to r5, r12 and lr as the caller left them, then the caller's
   stack from the stacked length up. Slot 0x18 is the surface's background
   colour, the getter DrawString itself asks. */
__attribute__((used)) static void text_record(const uint32_t *f)
{
    struct draw d;
    const char *str = (const char *)f[3];
    int len = (int)f[8], n;
    uint32_t surf = f[0];

    draw_seen++;
    if (!screen_hooked || surf == 0 || str == NULL)
        return;
    d.kind = EV_TEXT;
    d.tick = device_ticks();
    d.surf = surf;
    d.lr = f[7];
#ifndef SIM
    d.site = d.lr == WRAP_TEXT ? f[8 + 5] : d.lr == WRAP_CENTRE ? f[8 + 13]
           : d.lr == WRAP_PLAIN ? f[8 + 7] : 0;
#else
    d.site = sim_site;
#endif
    d.mark = ((int32_t (*)(uint32_t))(*(const uint32_t *const *)surf)[SLOT_MARK / 4])(surf);
    d.x = (int16_t)f[1];
    d.y = (int16_t)f[2];
    d.x1 = d.y1 = 0;
    d.task = (uint8_t)kernel_task_self();
    d.len = (uint8_t)(len < 0 ? 0 : len > 255 ? 255 : len);
    for (n = 0; n < len && n < DRAW_TEXT - 1 && str[n]; n++)
        d.text[n] = str[n];
    d.text[n] = 0;
    push(&d);
}

__attribute__((used)) static void clear_record(const uint32_t *f)
{
    struct draw d;

    if (!screen_hooked || f[0] == 0)
        return;
    memset(&d, 0, sizeof d);
    d.kind = EV_CLEAR;
    d.tick = device_ticks();
    d.surf = f[0];
    push(&d);
}

/* The rectangle is four inclusive corners, x0, y0, x1, y1, as the fill
   clips it against the surface's bounds. */
__attribute__((used)) static void fill_record(const uint32_t *f)
{
    struct draw d;
    const int32_t *r = (const int32_t *)f[1];

    if (!screen_hooked || f[0] == 0 || r == NULL)
        return;
    memset(&d, 0, sizeof d);
    d.kind = EV_FILL;
    d.tick = device_ticks();
    d.surf = f[0];
    d.x = (int16_t)r[0];
    d.y = (int16_t)r[1];
    d.x1 = (int16_t)r[2];
    d.y1 = (int16_t)r[3];
    push(&d);
}

/* Plain data writes, so the fault catcher may call this in handler mode. */
void screen_remove(void)
{
    unsigned i;

    if (!screen_hooked)
        return;
    for (i = 0; i < VTABLES; i++) {
        VT(i, SLOT_TEXT) = DRAW_STRING;
        VT(i, SLOT_CLEAR) = CLEAR_ALL;
        VT(i, SLOT_FILL) = FILL_RECT;
    }
    __asm__ volatile("dsb" ::: "memory");
    screen_hooked = 0;
}

/* The screen as drawn: one item per surface and position. */
#define ITEMS 512
#define ITEM_TEXT DRAW_TEXT
struct item {
    uint32_t surf, lr, site, first_drawn, last_drawn, last_change, erased, draws, seq;
    int32_t  mark;
    int16_t  x, y;
    uint8_t  used, changed, rapid, task, title, fresh, role;
    char     prev0;                          /* the text's first letter before */
    char     text[ITEM_TEXT];
};
static struct item items[ITEMS];
static int burst[64], nburst;
static uint32_t settle_ticks, batches, evicted, change_seq;
static uint32_t last_change, first_pending, last_popup_change;
static const struct item *last_label;
static int pending, wiped;

/* The background colour: white, 0xFFFFFF, on the status bar and on the
   focused item of a menu or grid, 0 or 0x1000000 elsewhere, -1 on the
   pop-up layers. */
#define WHITE 0xFFFFFF
#define STEADY 900u                          /* ticks, 1.2 s */
#define POPUP_LIFE 2250u                     /* ticks, 3 s */

/* The draw log: every change of text or background in full; at the end a
   count of each item's redraws, so a screen that redraws continually costs
   a line per item.
   Text leaving the screen is logged when it is taken off the model, not at
   each clear: some screens clear and redraw twenty-five times a second. */
#define LOG_CAP  (384u * 1024u)
#define SUMMARY  (48u * 1024u)
static char draw_log[LOG_CAP];
static size_t log_len;
static uint32_t log_dropped;

static void log_line(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

static void log_line(const char *fmt, ...)
{
    va_list ap;
    int n;

    if (log_len + 160 > LOG_CAP - SUMMARY) {
        log_dropped++;
        return;
    }
    va_start(ap, fmt);
    n = vsnprintf(draw_log + log_len, LOG_CAP - SUMMARY - log_len, fmt, ap);
    va_end(ap);
    if (n > 0 && (size_t)n < LOG_CAP - SUMMARY - log_len)
        log_len += (size_t)n;
}

int screen_install(int mode, int settle_ms)
{
    unsigned i;

    for (i = 0; i < VTABLES; i++)
        if (VT(i, SLOT_TEXT) != DRAW_STRING || VT(i, SLOT_CLEAR) != CLEAR_ALL
            || VT(i, SLOT_FILL) != FILL_RECT) {
            printf("screen: vtable %08lx reads %08lx %08lx %08lx, not the drawing calls\n",
                   (unsigned long)vt_base[i], (unsigned long)VT(i, SLOT_TEXT),
                   (unsigned long)VT(i, SLOT_CLEAR), (unsigned long)VT(i, SLOT_FILL));
            return -1;
        }
    screen_mode = mode;
    settle_ticks = MS_TO_TICKS(settle_ms);
    draw_rd = draw_wr;
    pending = 0;
    screen_hooked = 1;
    for (i = 0; i < VTABLES; i++) {
        VT(i, SLOT_TEXT) = (uint32_t)(uintptr_t)text_hook | 1u;
        VT(i, SLOT_CLEAR) = (uint32_t)(uintptr_t)clear_hook | 1u;
        VT(i, SLOT_FILL) = (uint32_t)(uintptr_t)fill_hook | 1u;
    }
    __asm__ volatile("dsb" ::: "memory");
    log_line("# tick task surface x y colour len caller site |text|, from %lu ticks,"
             " mode %d, settle %d ms\n", (unsigned long)device_ticks(), mode, settle_ms);
    return 0;
}

static size_t text_len(const struct item *it)
{
    return it->text[0] == 1 ? 0 : strlen(it->text);
}

/* A string's horizontal extent, at four pixels a character: the grid's
   seven-character cells sit thirty pixels apart, so the narrowest font is no
   wider than that. */
static int overlaps(const struct item *it, const struct draw *d)
{
    int a0 = it->x, a1 = it->x + 4 * (int)text_len(it);
    int b0 = d->x, b1 = d->x + 4 * (int)(d->len ? d->len : strlen(d->text));

    return a0 < b1 && b0 < a1;
}

static int live(const struct item *it)
{
    const char *t = it->text;

    if (!it->used || it->erased || text_len(it) == 0 || it->role == ROLE_IGNORE)
        return 0;
    while (*t == ' ')
        t++;
    return *t != 0;
}

/* The item a draw belongs to: the one at its surface and position, or a new
   one. A string drawn by the same code over the space an old one took, on
   the same line, is that item moved, as a centred title or a right-aligned
   value moves when its text changes length, unless the old one was drawn
   in this same frame and is simply its neighbour. Drawn by other code, it
   is new text in the old one's place, as a new screen's is. */
static struct item *find(const struct draw *d)
{
    struct item *free_one = NULL, *oldest = NULL;
    int i;

    for (i = 0; i < ITEMS; i++) {
        struct item *it = &items[i];
        if (it->used && it->surf == d->surf && it->x == d->x && it->y == d->y)
            return it;
    }
    for (i = 0; i < ITEMS; i++) {
        struct item *it = &items[i];
        if (it->used && it->surf == d->surf && it->y == d->y && it->site == d->site
            && d->tick - it->last_drawn > 2u && overlaps(it, d)) {
            if (free_one == NULL) {
                free_one = it;
                free_one->x = d->x;
            } else {
                it->used = 0;
            }
        }
    }
    /* A value redrawn a few pixels over as its width changes is the same
       value, with its history. */
    if (free_one != NULL)
        return free_one;
    for (i = 0; i < ITEMS; i++) {
        struct item *it = &items[i];
        if (!it->used) {
            if (free_one == NULL)
                free_one = it;
            continue;
        }
        if (oldest == NULL || (int32_t)(it->last_drawn - oldest->last_drawn) < 0)
            oldest = it;
    }
    if (free_one == NULL) {
        free_one = oldest;
        evicted++;
    }
    memset(free_one, 0, sizeof *free_one);
    free_one->used = 1;
    free_one->fresh = 1;
    free_one->first_drawn = d->tick;
    free_one->surf = d->surf;
    free_one->x = d->x;
    free_one->y = d->y;
    free_one->text[0] = 1;                   /* never equal to a real string */
    return free_one;
}

/* Roles, from how a string is drawn. */
static int is_title(const struct item *it)
{
    return it->title;
}

static int is_status(const struct item *it)
{
    return it->y <= 5 && it->mark == WHITE && !it->title;
}

static int is_lit(const struct item *it)
{
    return it->mark == WHITE && it->y >= 10 && !it->title;
}

static int is_popup(const struct item *it)
{
    return (it->mark == -1 || it->role == ROLE_TOAST) && !it->title;
}

/* The main screen's bank and pad, "A-13", banks A to J: its bank changing
   is said, a pad hit alone is not. "P-2" beside it is the pattern. */
static int is_pad_field(const struct item *it)
{
    const char *t = it->text;
    int digits = 0;

    if (it->role == ROLE_PAD)
        return 1;
    if (!is_status(it) || t[0] < 'A' || t[0] > 'J' || t[1] != '-')
        return 0;
    for (t += 2; *t >= '0' && *t <= '9'; t++)
        digits++;
    return *t == 0 && digits >= 1 && digits <= 3;
}

/* A pad as the screens name it, "A-13", "A 1" or "A14", banks A to J. */
static int padlike(const char *t)
{
    int digits = 0;

    if (t[0] < 'A' || t[0] > 'J')
        return 0;
    t += t[1] == '-' || t[1] == ' ' ? 2 : 1;
    for (; *t >= '0' && *t <= '9'; t++)
        digits++;
    return *t == 0 && digits >= 1 && digits <= 3;
}

static void erase(uint32_t surf, int x0, int y0, int x1, int y1, int whole, uint32_t tick)
{
    int i, n = 0;

    /* Half the screen or more: the kind of wipe a new screen starts with. */
    if (whole || (x1 - x0 + 1) * (y1 - y0 + 1) >= 128 * 64 / 2)
        wiped = 1;
    for (i = 0; i < ITEMS; i++) {
        struct item *it = &items[i];
        if (!it->used || it->erased || it->surf != surf)
            continue;
        if (whole || (it->x >= x0 && it->x <= x1 && it->y >= y0 && it->y <= y1)) {
            it->erased = tick ? tick : 1;
            n++;
        }
    }
    (void)n;
}

/* Folds one event into the model and answers whether it is a change worth
   waiting for: new text, or an item newly focused, and not a value changing
   so fast it waits to be heard until it holds still. A redraw that changes
   nothing is not a change, and nor is text erased and drawn straight back:
   some screens redraw every item continually. */
static int take(const struct draw *d)
{
    struct item *it;
    int was_lit, now_lit, text_changed, counts = 0;

    if (d->kind == EV_CLEAR) {
        erase(d->surf, 0, 0, 0, 0, 1, d->tick);
        return 0;
    }
    if (d->kind == EV_FILL) {
        erase(d->surf, d->x, d->y, d->x1, d->y1, 0, d->tick);
        return 0;
    }
    it = find(d);
    was_lit = is_lit(it);
    it->erased = 0;
    it->draws++;
    it->last_drawn = d->tick;
    it->lr = d->lr;
    it->site = d->site;
    it->task = d->task;
    it->role = role_of(d->site);
    it->title = it->role == ROLE_TITLE;
    text_changed = strcmp(it->text, d->text) != 0;
    if (text_changed || it->mark != d->mark)
        log_line("%lu %u %08lx %d %d %ld %u %08lx %08lx |%s|\n", (unsigned long)d->tick,
                 (unsigned)d->task, (unsigned long)d->surf, d->x, d->y, (long)d->mark,
                 (unsigned)d->len, (unsigned long)d->lr, (unsigned long)d->site, d->text);
    it->mark = d->mark;
    now_lit = is_lit(it);
    if (text_changed) {
        /* A first change is said at once. One that follows another within
           STEADY neither interrupts nor is said until the item has held still
           that long: a value being turned is heard where it stops, and a
           meter or a clock that never stops is not heard at all. A pad, and
           the main screen's status, change only when something is done,
           however quickly, so they are dealt with at once. */
        if (it->last_change != 0 && d->tick - it->last_change < STEADY && !padlike(d->text)
            && it->role != ROLE_MAIN && it->role != ROLE_PAD) {
            if (it->rapid < 255)
                it->rapid++;
        } else {
            it->rapid = 0;
        }
        it->last_change = d->tick;
        it->prev0 = it->text[0];
        memcpy(it->text, d->text, ITEM_TEXT);
        it->changed = 1;
        it->seq = ++change_seq;
        if (is_popup(it))
            last_popup_change = d->tick;
        counts = it->rapid == 0 || now_lit;
    } else if (now_lit && !was_lit) {
        it->changed = 2;
        it->seq = ++change_seq;
        counts = 1;
    }
    if (screen_mode == SCREEN_ALL) {
        int k = (int)(it - items);
        if (nburst < (int)(sizeof burst / sizeof burst[0])
            && (nburst == 0 || burst[nburst - 1] != k))
            burst[nburst++] = k;
        counts = 1;
    }
    return counts;
}

/* A string worth saying: runs of spaces closed up, the ends trimmed, a
   trailing ".." of truncation dropped, text spaced out a letter at a time
   ("9 4", "R E C") closed up, and at least one letter or digit in it. */
static size_t clean(char *out, size_t cap, const char *in)
{
    size_t n = 0, i, k;
    int alnum = 0, space = 0, spaced = 1;

    for (; *in && n + 1 < cap; in++) {
        unsigned char c = (unsigned char)*in;
        if (c < 0x20 || c == ' ') {
            space = n > 0;
            continue;
        }
        if (space && n + 2 < cap)
            out[n++] = ' ';
        space = 0;
        if ((c >= '0' && c <= '9') || ((c | 0x20) >= 'a' && (c | 0x20) <= 'z'))
            alnum = 1;
        out[n++] = (char)c;
    }
    out[n] = 0;
    while (n >= 3 && out[n - 1] == '.' && out[n - 2] == '.')
        out[n -= 2] = 0;
    while (n > 0 && out[n - 1] == ' ')
        out[--n] = 0;
    /* A page count, "1/ 5", as "1 of 5". */
    {
        size_t d1 = 0, j;
        while (d1 < n && out[d1] >= '0' && out[d1] <= '9')
            d1++;
        if (d1 > 0 && d1 < n && out[d1] == '/') {
            j = d1 + 1;
            while (j < n && out[j] == ' ')
                j++;
            if (j < n && out[j] >= '0' && out[j] <= '9') {
                size_t k2 = j;
                while (k2 < n && out[k2] >= '0' && out[k2] <= '9')
                    k2++;
                if (k2 == n && n + 4 < cap) {
                    char tail[16];
                    size_t tl = n - j < sizeof tail - 1 ? n - j : sizeof tail - 1;
                    memcpy(tail, out + j, tl);
                    tail[tl] = 0;
                    memcpy(out + d1, " of ", 4);
                    memcpy(out + d1 + 4, tail, tl + 1);
                    return strlen(out);
                }
            }
        }
    }
    for (i = 0; i < n; i++)
        if ((i % 2 == 1) != (out[i] == ' '))
            spaced = 0;
    if (spaced && n >= 3) {
        for (i = 0, k = 0; i < n; i += 2)
            out[k++] = out[i];
        out[n = k] = 0;
    }
    return alnum ? n : 0;
}

/* A grid cell cut short ("Crush..") whose full name ("Crusher") is also on
   the screen, as the title, answers the full name. */
static const struct item *full_name(const struct item *it)
{
    size_t n = strlen(it->text);
    int i;

    if (n < 3 || it->text[n - 1] != '.' || it->text[n - 2] != '.')
        return it;
    n -= 2;
    for (i = 0; i < ITEMS; i++) {
        const struct item *o = &items[i];
        if (live(o) && o != it && o->surf == it->surf && strlen(o->text) > n
            && strncmp(o->text, it->text, n) == 0 && o->text[n] != '.')
            return o;
    }
    return it;
}

/* The batch being built: phrases one after another, each ended by a zero. */
static char *b_out;
static size_t b_cap, b_used;
static int b_count;
static uint32_t b_now, b_prev;

static int in_batch(const char *text)
{
    const char *p = b_out;
    int i;

    for (i = 0; i < b_count; i++, p += strlen(p) + 1)
        if (strcmp(p, text) == 0)
            return 1;
    return 0;
}

/* Adds a phrase made of up to three texts. Answers 1 if added, 2 if the
   batch already says it, 0 if it says nothing or there is no room. */
static int say3(const char *a, const char *b, const char *c)
{
    char buf[3 * ITEM_TEXT], part[ITEM_TEXT];
    size_t n = 0;
    const char *src[3] = { a, b, c };
    int i;

    buf[0] = 0;
    for (i = 0; i < 3; i++) {
        size_t k;
        if (src[i] == NULL)
            continue;
        k = clean(part, sizeof part, src[i]);
        if (k == 0)
            continue;
        if (n > 0)
            buf[n++] = ' ';
        memcpy(buf + n, part, k + 1);
        n += k;
    }
    if (n == 0)
        return 0;
    if (in_batch(buf))
        return 2;
    if (b_used + n + 1 >= b_cap || b_count >= 24)
        return 0;
    memcpy(b_out + b_used, buf, n + 1);
    b_used += n + 1;
    b_count++;
    return 1;
}

static int say(const struct item *it)
{
    return say3(full_name(it)->text, NULL, NULL);
}

/* A pad, "A-13" or "A 1", as "A 13" or "A 1", written out directly:
   cleaning would close up "B 1" the way it closes up the big letter-spaced
   digits. */
static int say_pad(const struct item *it)
{
    char t[ITEM_TEXT];
    size_t n = strlen(it->text);

    if (n + 2 > sizeof t)
        return 0;
    if (it->text[1] == '-' || it->text[1] == ' ') {
        memcpy(t, it->text, n + 1);
    } else {
        t[0] = it->text[0];
        memcpy(t + 1, it->text, n + 1);
        t[1] = ' ';
        n++;
    }
    t[1] = ' ';
    if (in_batch(t))
        return 2;
    if (b_used + n + 1 >= b_cap || b_count >= 24)
        return 0;
    memcpy(b_out + b_used, t, n + 1);
    b_used += n + 1;
    b_count++;
    return 1;
}

/* Reading order: the screen before any pop-up over it, then top to bottom,
   then left to right. */
static int before(const struct item *a, const struct item *b)
{
    if (is_popup(a) != is_popup(b))
        return !is_popup(a);
    if (a->y != b->y)
        return a->y < b->y;
    return a->x < b->x;
}

static int in_order(struct item **out, int max, int (*want)(const struct item *))
{
    int n = 0, i, j;

    for (i = 0; i < ITEMS && n < max; i++) {
        struct item *it = &items[i];
        if (!live(it) || !want(it))
            continue;
        for (j = n; j > 0 && before(it, out[j - 1]); j--)
            out[j] = out[j - 1];
        out[j] = it;
        n++;
    }
    return n;
}

static int horizontally_near(const struct item *a, const struct item *b)
{
    int a0 = a->x - 4, a1 = a->x + 4 * (int)text_len(a) + 4;
    int b0 = b->x, b1 = b->x + 4 * (int)text_len(b);

    return a0 < b1 && b0 < a1;
}

static int plain(const struct item *it)
{
    return !is_title(it) && !is_status(it) && !is_lit(it) && it->role != ROLE_TABS;
}

/* A value's label is plain text drawn by other code: on its own line to
   its left and on another background, as the SYSTEM page sets "Edit Knob
   Mode" beside "Direct", or else just above it in its column, as the
   parameter pages set CUTOFF over 827. Text drawn by the same code is a
   neighbour, as a grid's cells are, and so is text beside it on the same
   background, as a menu's items are; neither is a label. */
static const struct item *row_label(const struct item *v)
{
    const struct item *best = NULL;
    int i;

    for (i = 0; i < ITEMS; i++) {
        const struct item *o = &items[i];
        if (!live(o) || o == v || o->surf != v->surf || !plain(o) || o->site == v->site
            || o->mark == v->mark || o->x >= v->x || o->y - v->y > 2 || v->y - o->y > 2)
            continue;
        if (best == NULL || o->x > best->x)
            best = o;
    }
    return best;
}

static const struct item *label_of(const struct item *v)
{
    const struct item *best = row_label(v);
    int i;

    if (best != NULL)
        return best;
    for (i = 0; i < ITEMS; i++) {
        const struct item *o = &items[i];
        if (!live(o) || o == v || o->surf != v->surf || !plain(o) || o->site == v->site
            || o->y >= v->y - 2 || v->y - o->y > 20 || !horizontally_near(v, o))
            continue;
        if (best == NULL || o->y > best->y)
            best = o;
    }
    return best;
}

/* A value's unit is short text just below it in its column, "Hz" under
   827; only a value with a label has one, and a value named on its own
   row, the next setting down, is not a unit. */
static const struct item *unit_of(const struct item *v)
{
    const struct item *best = NULL;
    char t[ITEM_TEXT];
    int i;

    if (label_of(v) == NULL)
        return NULL;
    for (i = 0; i < ITEMS; i++) {
        const struct item *o = &items[i];
        if (!live(o) || o == v || o->surf != v->surf || !plain(o) || o->y <= v->y
            || o->y - v->y > 20 || !horizontally_near(v, o))
            continue;
        if (clean(t, sizeof t, o->text) == 0 || strlen(t) > 4 || row_label(o) != NULL)
            continue;
        if (best == NULL || o->y < best->y)
            best = o;
    }
    return best;
}

/* A value with its label the first time it is heard, alone after that. */
static int say_value(const struct item *v)
{
    const struct item *l = label_of(v), *u;

    if (l != NULL && l != last_label) {
        last_label = l;
        u = unit_of(v);
        return say3(l->text, v->text, u ? u->text : NULL);
    }
    return say3(v->text, NULL, NULL);
}

/* A title drawn since the last batch. A title's layer may never be wiped,
   and UTILITY MENU read itself before every screen after it. */
static int want_title(const struct item *it)
{
    return is_title(it) && (int32_t)(it->last_drawn - b_prev) > 0;
}
static int want_lit(const struct item *it) { return is_lit(it); }
static int want_popup(const struct item *it) { return is_popup(it); }
static int want_new_popup(const struct item *it)
{
    return is_popup(it) && (it->fresh || it->changed == 1);
}
static int want_changed(const struct item *it) { return it->changed != 0; }
static int want_fresh(const struct item *it)
{
    return !is_status(it) && !is_title(it) && it->role != ROLE_TABS
        && (it->fresh || it->changed == 1);
}

/* A screen with no title and no focus: what it shows, each value read with
   its label and unit, and each label and unit used that way not read again
   on its own. */
static void say_contents(struct item **order, int n)
{
    int i, j, skip;

    for (i = 0; i < n; i++) {
        const struct item *it = order[i], *l = label_of(it);
        skip = 0;
        for (j = 0; j < n && !skip; j++) {
            const struct item *o = order[j];
            if (o == it)
                continue;
            if (unit_of(o) == it)
                skip = 1;
            else if (label_of(o) == it && unit_of(it) != o)
                skip = 1;
        }
        if (skip || (l != NULL && unit_of(l) == it))
            continue;
        if (l != NULL) {
            const struct item *u = unit_of(it);
            last_label = l;
            say3(l->text, it->text, u ? u->text : NULL);
        } else {
            say(it);
        }
    }
}

/* What to say about a new screen: its title, then its focus; on the main
   screen, which has neither, the bank and pad; then any pop-up; and on a
   screen with no title, focus or pop-up, what it shows. */
static void new_screen(struct item **order)
{
    int n, i, titles, focus, popups;

    last_label = NULL;
    n = in_order(order, ITEMS, want_title);
    for (i = 0; i < n; i++)
        say(order[i]);
    titles = b_count;
    (void)titles;
    /* The focus counts when it is there, even when the title has already
       said its words, as a grid's title names the focused cell. */
    focus = in_order(order, ITEMS, want_lit);
    for (i = 0; i < focus; i++)
        say(order[i]);
    if (focus == 0)
        for (i = 0; i < ITEMS; i++)
            if (live(&items[i]) && is_pad_field(&items[i]))
                focus += say_pad(&items[i]) == 1;
    popups = in_order(order, ITEMS, want_new_popup);
    for (i = 0; i < popups; i++)
        say(order[i]);
    if (focus == 0 && popups == 0) {
        n = in_order(order, ITEMS, want_fresh);
        say_contents(order, n);
    }
}

/* A pop-up layer that has only just been drawn: nothing on it older than
   this batch. */
static int layer_is_new(uint32_t surf)
{
    int i;

    for (i = 0; i < ITEMS; i++)
        if (live(&items[i]) && items[i].surf == surf && !items[i].fresh)
            return 0;
    return 1;
}

/* Text that only repeats the focus, as a title does that names the focused
   grid cell in full. */
static int mirrors_focus(const struct item *it)
{
    char a[ITEM_TEXT], b[ITEM_TEXT];
    int i;

    if (clean(a, sizeof a, it->text) == 0)
        return 0;
    for (i = 0; i < ITEMS; i++) {
        const struct item *f = &items[i];
        if (f != it && live(f) && is_lit(f) && clean(b, sizeof b, full_name(f)->text) > 0
            && strcmp(a, b) == 0)
            return 1;
    }
    return 0;
}

/* What to say about a change on the same screen: a message, whenever it is
   drawn; a pop-up that has just appeared, whole; the newly focused item;
   values that changed. On the main screen a pad hit says nothing, and nor
   do the other status fields it changes, but a bank change says the bank
   and pad; elsewhere a pad is named whenever it changes. Text that has
   only appeared, with nothing changed, stays quiet: a screen drawing the
   rest of itself, a meter's scale. */
static void same_screen(struct item **order)
{
    static struct item *pop[ITEMS];
    int n, i, k, m, pad_hit = 0;
    uint32_t popup_surf = 0;

    n = in_order(order, ITEMS, want_changed);
    for (i = 0; i < n; i++) {
        struct item *it = order[i];
        if (!is_popup(it) || it->changed != 1)
            continue;
        if (it->role == ROLE_TOAST) {
            say(it);
            it->changed = 0;
        } else if (popup_surf != it->surf && layer_is_new(it->surf)) {
            popup_surf = it->surf;
            m = in_order(pop, ITEMS, want_popup);
            for (k = 0; k < m; k++)
                if (pop[k]->surf == popup_surf) {
                    const struct item *l = label_of(pop[k]);
                    say(pop[k]);
                    pop[k]->changed = 0;
                    if (l != NULL)
                        last_label = l;
                }
        }
    }
    for (i = 0; i < n; i++)
        if (order[i]->changed && is_lit(order[i]) && say(order[i]))
            order[i]->changed = 0;
    for (i = 0; i < n; i++)
        if (order[i]->changed == 1 && is_pad_field(order[i]))
            pad_hit = 1;
    for (i = 0; i < n; i++) {
        struct item *it = order[i];
        if (it->changed != 1 || is_lit(it) || is_title(it))
            continue;
        if (mirrors_focus(it)) {
            it->changed = 0;
            continue;
        }
        if (it->rapid > 0 && b_now - it->last_change < STEADY)
            continue;
        if (is_pad_field(it)) {
            if (it->prev0 != it->text[0])
                say_pad(it);
        } else if (padlike(it->text) && is_status(it)) {
            say_pad(it);
        } else if (it->fresh) {
            /* appeared, not changed */
        } else if (it->role == ROLE_MAIN || is_status(it)) {
            if (!(it->role == ROLE_MAIN && pad_hit) && b_now - last_popup_change >= 750u)
                say(it);
        } else {
            say_value(it);
        }
        it->changed = 0;
    }
}

/* A new screen: its title changed; or a surface that was not showing
   anything is now showing three strings or more; or after a wipe of half
   the screen or more, at least three strings are new and they make up half
   of what is showing. A pad hit wipes and redraws the status line, which
   changes but is not new; a screen drawing the rest of itself a moment
   later, with no wipe, is not a new screen. */
static int screen_changed(void)
{
    uint32_t surf[16];
    int nsurf = 0, i, k, alive = 0, fresh = 0;

    for (i = 0; i < ITEMS; i++) {
        const struct item *it = &items[i];
        if (!live(it) || is_popup(it))
            continue;
        if (is_title(it) && it->changed == 1)
            return 1;
        alive++;
        if (it->fresh || (it->changed == 1 && !is_status(it)))
            fresh++;
        for (k = 0; k < nsurf && surf[k] != it->surf; k++)
            ;
        if (k == nsurf && nsurf < 16)
            surf[nsurf++] = it->surf;
    }
    for (k = 0; k < nsurf; k++) {
        int n = 0;
        if (!layer_is_new(surf[k]))
            continue;
        for (i = 0; i < ITEMS; i++)
            if (live(&items[i]) && items[i].surf == surf[k])
                n++;
        if (n >= 3)
            return 1;
    }
    return wiped && fresh >= 3 && fresh * 2 >= alive;
}

/* Drains what the hooks caught. Once there has been no change for the
   settle time, or changes have kept coming for a second, or a value that was
   changing quickly has held still, fills phrases with what to say and
   answers 1. */
int screen_poll(char *phrases, size_t cap, int *count)
{
    static struct item *order[ITEMS];
    uint32_t now = device_ticks();
    int i, due = 0, kind = 0;

    while (draw_rd != draw_wr) {
        if (take(&draws[draw_rd % DRAWS])) {
            if (!pending)
                first_pending = now;
            pending = 1;
            last_change = now;
        }
        __asm__ volatile("dmb" ::: "memory");
        draw_rd++;
    }
    if (pending && (now - last_change >= settle_ticks || now - first_pending >= 750u))
        due = 1;
    for (i = 0; i < ITEMS && !due; i++)
        if (items[i].used && items[i].changed == 1 && items[i].rapid > 0
            && now - items[i].last_change >= STEADY)
            due = 1;
    if (!due)
        return 0;
    /* What was erased and not drawn again is gone, and so is a pop-up not
       drawn for three seconds: a pop-up's layer is not cleared when it
       closes, so its text would otherwise stay on the model for good. */
    for (i = 0; i < ITEMS; i++) {
        struct item *it = &items[i];
        if (!it->used)
            continue;
        if ((it->erased && now - it->erased > 2u)
            || (is_popup(it) && now - it->last_drawn > POPUP_LIFE)) {
            if (text_len(it) > 0 && it->last_drawn - it->first_drawn >= 750u)
                log_line("%lu gone %08lx %d %d |%s|\n", (unsigned long)now,
                         (unsigned long)it->surf, it->x, it->y, it->text);
            it->used = 0;
        }
    }
    pending = 0;
    b_out = phrases;
    b_cap = cap;
    b_used = 0;
    b_count = 0;
    b_now = now;
    if (screen_mode == SCREEN_ALL) {
        for (i = 0; i < nburst; i++)
            say(&items[burst[i]]);
        nburst = 0;
        for (i = 0; i < ITEMS; i++)
            items[i].changed = 0;
    } else if ((kind = screen_changed()) != 0) {
        new_screen(order);
        for (i = 0; i < ITEMS; i++)
            items[i].changed = 0;
    } else {
        same_screen(order);
        for (i = 0; i < ITEMS; i++)
            if (items[i].changed == 2 || !items[i].used)
                items[i].changed = 0;
    }
    for (i = 0; i < ITEMS; i++)
        items[i].fresh = 0;
    wiped = 0;
    b_prev = now;
    *count = b_count;
#ifdef SIM
    sim_batch(phrases, b_count);
#endif
    if (b_count == 0)
        return 0;
    batches++;
    log_line("%lu say %s %d:", (unsigned long)now,
             screen_mode == SCREEN_ALL ? "all" : kind ? "new" : "same", b_count);
    {
        const char *p = phrases;
        for (i = 0; i < b_count; i++) {
            log_line(" |%s|", p);
            p += strlen(p) + 1;
        }
    }
    log_line("\n");
    return 1;
}

void screen_report(void)
{
    printf("screen: %lu draws seen, %lu lost, %lu batches, %lu items evicted, %lu log lines dropped\n",
           (unsigned long)draw_seen, (unsigned long)draw_lost, (unsigned long)batches,
           (unsigned long)evicted, (unsigned long)log_dropped);
}

/* The log so far, then every item on record with how often it was drawn. */
int screen_log_write(void)
{
    static size_t written;
    size_t n = log_len;
    int i, k;

    if (log_len == 0 || log_len == written)
        return 0;
    written = log_len;
    k = snprintf(draw_log + n, LOG_CAP - n,
                 "# items: surface x y draws colour task caller site erased |text|\n");
    if (k > 0)
        n += (size_t)k;
    for (i = 0; i < ITEMS && n + 128 < LOG_CAP; i++) {
        const struct item *it = &items[i];
        if (!it->used)
            continue;
        k = snprintf(draw_log + n, LOG_CAP - n, "%08lx %d %d %lu %ld %u %08lx %08lx %u |%s|\n",
                     (unsigned long)it->surf, it->x, it->y, (unsigned long)it->draws,
                     (long)it->mark, (unsigned)it->task, (unsigned long)it->lr,
                     (unsigned long)it->site, it->erased != 0,
                     it->text[0] == 1 ? "" : it->text);
        if (k > 0)
            n += (size_t)k;
    }
    write_file("A:/EVV/DRAWS.TXT", draw_log, n);
    return 1;
}
