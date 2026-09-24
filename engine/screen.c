/* What the screen draws, as text the engine can speak, and VALUE handed from
   the loader to the resident engine and back.

   Every string on the normal screens goes through one DrawString,
   FUN_800EE530(surface, x, y, string, length), reached only through seven
   vtable words. Pointing them at text_hook is a data write: no instruction
   changes, so the caches need nothing. text_hook runs inside the interface's
   own drawing, so it only copies the call into a ring and wakes the engine's
   task; it never waits, allocates or touches the card. Then it jumps on to
   the real DrawString with every register and the stack as they came.

   The engine's task drains the ring into a model of the screen keyed by
   surface and position, logs what it saw, and once the drawing has been
   quiet for the settle time hands back the strings to speak.

   The reboot page's key handler jumps to the loader through the literal
   word at 0x80142C24, read as data. Pointing that at key_handler makes VALUE
   ask the engine to unload instead of loading a second copy over it. */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "kernel.h"
#include "device.h"

#ifndef SIM
#define DRAW_STRING     0x800EE531u
#define DRAW_STRING_ASM "0x800EE531"
static volatile uint32_t *const vt_slots[] = {
    (volatile uint32_t *)0x8021D2D0u, (volatile uint32_t *)0x8021D46Cu,
    (volatile uint32_t *)0x8021D608u, (volatile uint32_t *)0x8021D7A4u,
    (volatile uint32_t *)0x8021E3D0u, (volatile uint32_t *)0x8021E564u,
    (volatile uint32_t *)0x8021E700u,
};
#define KEY_WORD (*(volatile uint32_t *)0x80142C24u)
#define LOADER   0x60308001u
#else
/* The simulator's stand-ins, from sim.c. */
extern volatile uint32_t sim_vtable[7], sim_key_word;
void sim_draw_string(void);
#define DRAW_STRING     ((uint32_t)(uintptr_t)sim_draw_string)
#define DRAW_STRING_ASM "sim_draw_string"
static volatile uint32_t *const vt_slots[] = {
    &sim_vtable[0], &sim_vtable[1], &sim_vtable[2], &sim_vtable[3],
    &sim_vtable[4], &sim_vtable[5], &sim_vtable[6],
};
#define KEY_WORD sim_key_word
#define LOADER   0x60308001u
#endif
#define VT_SLOTS (sizeof vt_slots / sizeof vt_slots[0])
#define KEY_VALUE 0x31

static volatile int key_hooked, unload_flag, screen_hooked, screen_mode;

static int key_handler(void *page, int key)
{
    (void)page;
    if (key == KEY_VALUE) {
        unload_flag = 1;
        target_wake();
    }
    return 0;
}

int key_install(void)
{
    if (KEY_WORD != LOADER) {
        printf("key: loader word reads %08lx, not the loader\n", (unsigned long)KEY_WORD);
        return -1;
    }
    KEY_WORD = (uint32_t)(uintptr_t)key_handler | 1u;
    __asm__ volatile("dsb" ::: "memory");
    key_hooked = 1;
    return 0;
}

void key_remove(void)
{
    if (key_hooked) {
        KEY_WORD = LOADER;
        __asm__ volatile("dsb" ::: "memory");
        key_hooked = 0;
    }
}

int unload_requested(void)
{
    return unload_flag;
}

/* One DrawString call as the hook saw it. */
#define DRAW_TEXT 64
struct draw {
    uint32_t tick, surf, lr;
    int32_t  mark;
    int16_t  x, y;
    uint8_t  task, len;
    char     text[DRAW_TEXT];
};

#define DRAWS 512
static struct draw draws[DRAWS];
static volatile uint32_t draw_wr, draw_rd, draw_lost, draw_seen;

void text_hook(void);
__asm__(
".syntax unified\n"
".thumb\n"
".text\n"
".globl text_hook\n"
".thumb_func\n"
".type text_hook, %function\n"
/* Eight words keep the stack 8-byte aligned for the C call, and leave the
   caller's fifth argument, the length, at sp + 32. */
"text_hook:\n"
"    push  {r0-r5, r12, lr}\n"
"    mov   r0, sp\n"
"    bl    text_record\n"
"    pop   {r0-r5, r12, lr}\n"
"    ldr   pc, =" DRAW_STRING_ASM "\n"
".ltorg\n"
);

/* f holds r0 to r5, r12 and lr as the caller left them, then the stacked
   length. Slot 0x18 is the plain getter DrawString itself asks before it
   marks the drawn area; what its value means is for the log to show. */
__attribute__((used)) static void text_record(const uint32_t *f)
{
    uint32_t surf = f[0], primask, w;
    const char *str = (const char *)f[3];
    int len = (int)f[8], n;
    int32_t mark;
    int task;
    struct draw *d;

    draw_seen++;
    if (!screen_hooked || surf == 0 || str == NULL)
        return;
    mark = ((int32_t (*)(uint32_t))(*(const uint32_t *const *)surf)[0x18 / 4])(surf);
    task = kernel_task_self();
    __asm__ volatile("mrs %0, primask\n cpsid i" : "=r"(primask) :: "memory");
    w = draw_wr;
    if (w - draw_rd >= DRAWS) {
        draw_lost++;
    } else {
        d = &draws[w % DRAWS];
        d->tick = device_ticks();
        d->surf = surf;
        d->lr = f[7];
        d->mark = mark;
        d->x = (int16_t)f[1];
        d->y = (int16_t)f[2];
        d->task = (uint8_t)task;
        d->len = (uint8_t)(len < 0 ? 0 : len > 255 ? 255 : len);
        for (n = 0; n < len && n < DRAW_TEXT - 1 && str[n]; n++)
            d->text[n] = str[n];
        d->text[n] = 0;
        draw_wr = w + 1;
    }
    __asm__ volatile("msr primask, %0" :: "r"(primask) : "memory");
    target_wake();
}

/* Plain data writes, so the fault catcher may call this in handler mode. */
void screen_remove(void)
{
    unsigned i;

    if (!screen_hooked)
        return;
    for (i = 0; i < VT_SLOTS; i++)
        *vt_slots[i] = DRAW_STRING;
    __asm__ volatile("dsb" ::: "memory");
    screen_hooked = 0;
}

/* The screen as last drawn: one item per surface and position. */
#define ITEMS 512
#define ITEM_TEXT DRAW_TEXT
struct item {
    uint32_t surf, lr, last_drawn, last_change, draws, seq;
    int32_t  mark;
    int16_t  x, y;
    uint8_t  used, changed, rapid, task;
    char     text[ITEM_TEXT];
};
static struct item items[ITEMS];
static int burst[64], nburst;
static uint32_t settle_ticks, batches, evicted, change_seq;
static uint32_t last_change, first_pending;
static int pending;

/* What the slot 0x18 getter answers looks like the background the text is
   drawn on: 0xFFFFFF, white, on the inverted status bar and on one item of
   each menu, 0 or 0x1000000 elsewhere, -1 on the pop-up layers. White away
   from the top bar is taken to be the menu's highlight. The first run with
   the hook showed only the white items; this is the guess the next run's
   log is there to confirm. */
#define HIGHLIGHT 0xFFFFFF

#define STEADY 900u                          /* ticks, 1.2 s */

/* The draw log: every change of text or background in full, and at the end
   a count of each item's redraws, so a screen that redraws continually costs
   a line per item, not thousands. */
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

    for (i = 0; i < VT_SLOTS; i++)
        if (*vt_slots[i] != DRAW_STRING) {
            printf("screen: vtable word %08lx reads %08lx, not DrawString\n",
                   (unsigned long)(uintptr_t)vt_slots[i], (unsigned long)*vt_slots[i]);
            return -1;
        }
    screen_mode = mode;
    settle_ticks = MS_TO_TICKS(settle_ms);
    draw_rd = draw_wr;
    pending = 0;
    screen_hooked = 1;
    for (i = 0; i < VT_SLOTS; i++)
        *vt_slots[i] = (uint32_t)(uintptr_t)text_hook | 1u;
    __asm__ volatile("dsb" ::: "memory");
    log_line("# tick task surface x y mark len caller |text|, from %lu ticks, mode %d, settle %d ms\n",
             (unsigned long)device_ticks(), mode, settle_ms);
    return 0;
}

/* A string's horizontal extent, at four pixels a character: the grid's
   seven-character cells sit thirty pixels apart, so the narrowest font is no
   wider than that. */
static int overlaps(const struct item *it, const struct draw *d)
{
    int a0 = it->x, a1 = it->x + 4 * (int)strlen(it->text);
    int b0 = d->x, b1 = d->x + 4 * (int)(d->len ? d->len : strlen(d->text));

    return a0 < b1 && b0 < a1;
}

/* The item a draw belongs to: the one at its surface and position, or a new
   one. A new string drawn over the space an old one took, on the same line,
   replaces it, as a centred title does when its text changes length, unless
   the old one was drawn in this same frame and is simply its neighbour. */
static struct item *find(const struct draw *d)
{
    struct item *free_one = NULL, *oldest = NULL, *at = NULL;
    int i;

    for (i = 0; i < ITEMS; i++) {
        struct item *it = &items[i];
        if (it->used && it->surf == d->surf && it->x == d->x && it->y == d->y) {
            at = it;
            break;
        }
    }
    if (at != NULL)
        return at;
    for (i = 0; i < ITEMS; i++) {
        struct item *it = &items[i];
        if (it->used && it->surf == d->surf && it->y == d->y
            && d->tick - it->last_drawn > 2u && overlaps(it, d))
            it->used = 0;
    }
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
    free_one->surf = d->surf;
    free_one->x = d->x;
    free_one->y = d->y;
    free_one->text[0] = 1;                   /* never equal to a real string */
    return free_one;
}

/* The status bar across the top is drawn on white too; a highlight is white
   below it. */
static int lit(int32_t mark, int y)
{
    return mark == HIGHLIGHT && y >= 10;
}

/* Folds one draw into the model and answers whether it is a change worth
   waiting for: new text, or an item newly highlighted, and not an item that
   is changing so fast it has been muted. A redraw that changes nothing is
   not a change: some screens redraw every item continually, and counting
   those kept the screen from ever settling. */
static int take(const struct draw *d)
{
    struct item *it = find(d);
    int was_lit = lit(it->mark, it->y), now_lit = lit(d->mark, d->y);
    int text_changed = strcmp(it->text, d->text) != 0;
    int counts = 0;

    it->draws++;
    it->last_drawn = d->tick;
    it->lr = d->lr;
    it->task = d->task;
    if (text_changed || it->mark != d->mark)
        log_line("%lu %u %08lx %d %d %ld %u %08lx |%s|\n", (unsigned long)d->tick,
                 (unsigned)d->task, (unsigned long)d->surf, d->x, d->y, (long)d->mark,
                 (unsigned)d->len, (unsigned long)d->lr, d->text);
    it->mark = d->mark;
    if (text_changed) {
        /* A first change is said at once. One that follows another within
           STEADY neither interrupts nor is said until the item has held still
           that long: a value being turned is heard where it stops, and a
           meter or a clock that never stops is not heard at all. */
        if (it->last_change != 0 && d->tick - it->last_change < STEADY) {
            if (it->rapid < 255)
                it->rapid++;
        } else {
            it->rapid = 0;
        }
        it->last_change = d->tick;
        memcpy(it->text, d->text, ITEM_TEXT);
        it->changed = 1;
        it->seq = ++change_seq;
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
        if (o->used && o != it && o->surf == it->surf && strlen(o->text) > n
            && strncmp(o->text, it->text, n) == 0 && o->text[n] != '.')
            return o;
    }
    return it;
}

/* Adds a phrase unless it says nothing, or is already in the batch: some
   values are drawn twice, and a full name may stand in for a cell. */
static int add_phrase(char *phrases, size_t cap, size_t *used, int *count, const struct item *it)
{
    char *at = phrases + *used;
    size_t n = clean(at, cap - *used, full_name(it)->text);
    const char *p = phrases;
    int i;

    if (n == 0)
        return 0;
    for (i = 0; i < *count; i++, p += strlen(p) + 1)
        if (strcmp(p, at) == 0)
            return 2;
    if (*used + n + 1 >= cap || *count >= 24)
        return 0;
    *used += n + 1;
    (*count)++;
    return 1;
}

static int popup(const struct item *it)
{
    return it->mark == -1;
}

/* Reading order: the screen before any pop-up over it, then top to bottom,
   then left to right. */
static int before(const struct item *a, const struct item *b)
{
    if (popup(a) != popup(b))
        return !popup(a);
    if (a->y != b->y)
        return a->y < b->y;
    return a->x < b->x;
}

static int changed_in_order(struct item **out, int max)
{
    int n = 0, i, j;

    for (i = 0; i < ITEMS && n < max; i++) {
        struct item *it = &items[i];
        /* Newly highlighted and already left again: scrolled past. */
        if (!it->used || !it->changed || (it->changed == 2 && !lit(it->mark, it->y)))
            continue;
        for (j = n; j > 0 && before(it, out[j - 1]); j--)
            out[j] = out[j - 1];
        out[j] = it;
        n++;
    }
    return n;
}

static int steady(const struct item *it, uint32_t now)
{
    return it->rapid == 0 || now - it->last_change >= STEADY;
}

/* Drains what the hook caught. Once there has been no change for the
   settle time, or changes have kept coming for a second, or a value that was
   changing quickly has held still, fills phrases with the strings to say and
   answers 1: the highlighted item first, then the rest of what changed in
   reading order. A value still changing quickly waits for a later batch,
   unless the batch already says its text. */
int screen_poll(char *phrases, size_t cap, int *count)
{
    static struct item *order[ITEMS];
    static char said[ITEMS];
    uint32_t now = device_ticks();
    size_t used = 0;
    int i, n, due = 0;

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
    pending = 0;
    *count = 0;
    if (screen_mode == SCREEN_ALL) {
        for (i = 0; i < nburst; i++)
            add_phrase(phrases, cap, &used, count, &items[burst[i]]);
        nburst = 0;
        for (i = 0; i < ITEMS; i++)
            items[i].changed = 0;
    } else {
        n = changed_in_order(order, ITEMS);
        for (i = 0; i < n; i++) {
            said[i] = 0;
            if (lit(order[i]->mark, order[i]->y))
                said[i] = (char)add_phrase(phrases, cap, &used, count, order[i]);
        }
        for (i = 0; i < n; i++)
            if (!lit(order[i]->mark, order[i]->y) && steady(order[i], now))
                said[i] = (char)add_phrase(phrases, cap, &used, count, order[i]);
        for (i = 0; i < n; i++) {
            struct item *it = order[i];
            if (!said[i] && !steady(it, now)) {
                char text[ITEM_TEXT];
                const char *p = phrases;
                int k, dup = 0;
                clean(text, sizeof text, full_name(it)->text);
                for (k = 0; k < *count; k++, p += strlen(p) + 1)
                    if (strcmp(p, text) == 0)
                        dup = 1;
                if (!dup)
                    continue;
            }
            it->changed = 0;
        }
        for (i = 0; i < ITEMS; i++)
            if (items[i].changed == 2 || !items[i].used)
                items[i].changed = 0;
    }
    if (*count == 0)
        return 0;
    batches++;
    log_line("%lu say %d:", (unsigned long)now, *count);
    {
        const char *p = phrases;
        for (i = 0; i < *count; i++) {
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
    k = snprintf(draw_log + n, LOG_CAP - n, "# items: surface x y draws mark task caller |text|\n");
    if (k > 0)
        n += (size_t)k;
    for (i = 0; i < ITEMS && n + 128 < LOG_CAP; i++) {
        const struct item *it = &items[i];
        if (!it->used)
            continue;
        k = snprintf(draw_log + n, LOG_CAP - n, "%08lx %d %d %lu %ld %u %08lx |%s|\n",
                     (unsigned long)it->surf, it->x, it->y, (unsigned long)it->draws,
                     (long)it->mark, (unsigned)it->task, (unsigned long)it->lr,
                     it->text[0] == 1 ? "" : it->text);
        if (k > 0)
            n += (size_t)k;
    }
    write_file("A:/EVV/DRAWS.TXT", draw_log, n);
    return 1;
}
