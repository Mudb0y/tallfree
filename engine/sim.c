/* The resident service under QEMU, with the instrument simulated round it.

   Time is virtual and moves only when the service sleeps, 0.75 audio ticks
   and 11.025 samples of playback to the millisecond. Draws, clears and VALUE
   presses come from sim_draws.txt at their times, through the same vtable
   words the instrument's interface would call through, so the hooks, the
   model, the cut-offs and the unload path all run as they would on the unit.
   Settings and the script come from sim_say.txt, files the service writes
   land beside the run as sim_*.TXT, and what was played goes to sim.raw.

   sim_draws.txt, one event a line; MS is ms after screen reading goes live,
   or after the start with @ before it, and sN picks surface N, 0 to 3:
     MS [sN] X Y TEXT          drawn; TEXT may start with flags:
                               ! on white, ~ on a pop-up layer, _ on black,
                               ^ drawn by the page-title setter,
                               =XXXXXXXX: drawn from that site
     MS [sN] CLEAR             the surface cleared
     MS [sN] FILL X0 Y0 X1 Y1  a rectangle cleared
     MS [sN] BOX X0 Y0 X1 Y1 [=XXXXXXXX]
                               a rectangle's outline, drawn from that site
     MS [sN] ICON X Y SEL NAME an icon menu's icon, selected if SEL is 1
     MS [sN] ROW Y SEL         a settings row whose text is at Y, selected if 1
     MS [sN] TAB CUR A,B,...   a tab strip drawn, tab CUR of those current
     MS [sN] SCROLL K X Y TEXT a name scrolling in a list, drawn from its
                               Kth character, flags as for a draw; the list
                               keeps it whole, and the page K, as the import
                               list and REMAIN do
     MS PAGE N                 page factory N called: a page being built
     MS KEY DOWN|UP HEX        a key sent to the page last built, 84 at first
     MS KNOB N STEP            knob N turned by STEP, to that page
     MS CTRL N POS             CTRL knob N moved to POS, 0 to 127, to that page
     MS PAD N                  pad N, 1 to 16, pressed, to the page last built
     MS MODE M                 the page last built is in mode M
     MS BANK B                 the current bank is B, 0 to 9
     MS PMODE M                the pattern store's mode is M, 8 for COPY
     MS PKEEP 0|1              COPY choosing the samples to keep, or not
     MS TRREC NOTE             TR-REC inputting NOTE, 47 for A 1, or 0 for
                               not in TR-REC; its pads set steps 60 ms on
     MS VALUE                  the run ends: speech off and unload

   sim_expect.txt, if present, holds what each spoken batch should be, one
   batch a line, phrases separated by " | "; the run ends by saying whether
   they matched. */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "device.h"

int  sh_open(const char *path, int mode);
int  sh_write(int fd, const void *buf, int len);
int  sh_close(int fd);
int  sh_read(int fd, void *buf, int len);
int  sh_flen(int fd);

#define TITLE_SITE 0x80081011u

volatile uint32_t sim_vtables[7][0x130 / 4], sim_site;
volatile uint32_t sim_icon_slots[2], sim_icon_ctx;
volatile uint32_t sim_page_table[94], sim_row_word, sim_tab_word;

/* The firmware's record of what the screens that ask for pads have chosen,
   the store at 0x82E01144, the current bank, and the export and pad
   operations pages, each with its mode at 0x1A90. */
volatile uint32_t sim_store[0x600 / 4], sim_export_page, sim_padops_page;
volatile int32_t sim_bank;
/* Each pad's playback settings, 0xAC bytes a pad from 0x82E2CD08, and the
   current pad in its bank. */
volatile uint32_t sim_padstore[160 * 0xAC / 4 + 16];
volatile int32_t sim_padnow;
/* The pattern sequencer's store at 0x82DFFC88, and whether COPY is choosing
   the samples to keep, the flag at 0x2A of the store at 0x82E01144. */
volatile uint32_t sim_patstore[0x800 / 4];
static int sim_keep;
/* The sequencer's state, which 0x80591F60 points at, and TR-REC's steps for
   the bar shown as FUN_80034110 reads them: sixteen steps of four fine
   slots, 256 bytes a slot. A pad in TR-REC sets or clears its step a moment
   after it is pressed, as the sequencer takes the note; with SUB PAD held
   it chooses the sample instead, and with PATTERN EDIT held it opens the
   Microscope. */
static int16_t sim_seq[0x60 / 2];
volatile uint32_t sim_seq_state;
volatile int32_t sim_step_count = 16, sim_step_slots = 64, sim_step_last = 64;
volatile uint16_t sim_step_bits[64 * 128];
static int sim_subpad, sim_patedit, step_due_pad = -1;
static uint32_t step_due_ms;
#define PSTORE(off) (*(int32_t *)((char *)sim_patstore + (off)))
static int sim_shift;
static uint32_t page_object[0x1AA0 / 4];
/* What a scrolling name's drawing code keeps in r5: the import list, with
   the whole name at +0x35C, or the REMAIN page, with how far it has
   scrolled at +0x1A8C. */
static uint32_t scroll_owner[0x1A90 / 4];
volatile uint32_t sim_scroll_owner;
static int page_now = 84;
#define PAGE_MODE (*(int32_t *)((char *)page_object + 0x1A90))

void sim_draw_row(void *page, void *ctx, const int16_t *rect, int a, int b, int sel, int c)
{
    (void)page; (void)ctx; (void)rect; (void)a; (void)b; (void)sel; (void)c;
}

/* A tab widget as FUN_80121478 reads one: the current index at +0x320, the
   count at +0x328, the names from +0x29C, the drawing context at +0x84. */
void sim_draw_tabs(void *widget) { (void)widget; }
static uint32_t tab_widget[0x340 / 4], tab_ctx[8];
static char tab_names[16][24];

static unsigned page_saw_keys;

/* Every page, as far as the reader can tell: a page built puts itself
   where the firmware keeps it, and a pad pressed where a screen asks for
   pads changes the choice as FUN_80151688 and FUN_8012E0B8 do. */
static int sim_page_factory(void *request)
{
    const int16_t *m = request;
    uint8_t *store = (uint8_t *)sim_store;
    int pad, index;

    if (m == NULL)
        return 0;
    if (*m >= 5 && *m <= 7)
        page_saw_keys++;
    if ((*m == 5 || *m == 6) && m[1] == 0x2A)
        sim_shift = *m == 5;
    if ((*m == 5 || *m == 6) && m[1] == 0x13)
        sim_subpad = *m == 5;
    if ((*m == 5 || *m == 6) && m[1] == 0x15)
        sim_patedit = *m == 5;
    /* EXT SOURCE as FUN_80134A98 sets the input, the word at 0x5C of the
       store: each press flips it. */
    if (*m == 5 && m[1] == 0x12 && !sim_shift)
        sim_store[0x5C / 4] = !sim_store[0x5C / 4];
    /* The playback buttons as FUN_800C9788 and FUN_801339C8 set the
       current pad: BPM SYNC, GATE, REVERSE flip; LOOP turns on, with SHIFT
       the ping pong loop, and off. */
    if (*m == 5 && m[1] >= 0x1D && m[1] <= 0x20) {
        uint8_t *r = (uint8_t *)sim_padstore + (sim_bank * 16 + sim_padnow) * 0xAC;
        uint32_t *gate = (uint32_t *)(r + 0xE0), *loop = (uint32_t *)(r + 0xE4);
        uint32_t *sync = (uint32_t *)(r + 0xF0), *flags = (uint32_t *)(r + 0x10C);
        uint32_t v, mode;
        switch (m[1]) {
        case 0x1D:
            *sync = !*sync;
            break;
        case 0x1E:
            *gate = !*gate;
            break;
        case 0x1F:
            v = (*flags & 0xFF) | (*loop << 2);
            mode = (v & 0xFF) >> 1;
            if (mode == 3)
                v = *flags & 1;
            else if (mode == 2)
                v = (*flags & 1) | (uint32_t)sim_shift << 1 | (uint32_t)sim_shift << 2;
            else if (mode == 0)
                v = (*flags & 1) + (sim_shift ? 6 : 4);
            *loop = v >> 2;
            *flags = v & 3;
            break;
        case 0x20:
            if (!sim_shift)
                *flags ^= 1;
            break;
        }
    }
    /* VALUE on COPY BANK PAD moves its cursor, the word at 0x488 of the
       store, to the destination and back; on the pattern screen's COPY BANK
       the word at 0x2EC of the pattern store. */
    if (*m == 7 && m[1] == 0 && page_now == 67 && PAGE_MODE == 3)
        sim_store[0x488 / 4] = m[2] > 0;
    if (*m == 7 && m[1] == 0 && page_now == 60 && PSTORE(0x34) == 9)
        PSTORE(0x2EC) = m[2] > 0;
    /* The bank keys in pattern mode, as FUN_80084258 and FUN_800C9788 set
       the pattern store: the bank the pads show; once COPY or EXCHANGE has
       a source, the destination's, or the samples' to keep; on COPY BANK
       the bank the cursor is on. A key pressed again gives the other bank
       of its pair. */
    if (*m == 5 && m[1] >= 0x25 && m[1] <= 0x29 && PSTORE(0x34) > 0) {
        int k = m[1] - 0x25, mode = PSTORE(0x34);
        uint32_t off = 0x2BC;
        if (page_now == 60 && mode == 9)
            off = PSTORE(0x2EC) ? 0x2E4 : 0x2E8;
        else if (page_now == 60 && (mode == 8 || mode == 10) && PSTORE(0x2B8) >= 0)
            off = mode == 8 && sim_keep ? 0x2F0 : 0x2DC;
        PSTORE(off) = PSTORE(off) == k ? k + 5 : k;
    }
    if (*m == 1) {
        sim_export_page = page_now == 85 ? (uint32_t)(uintptr_t)page_object : 0;
        sim_padops_page = page_now == 67 ? (uint32_t)(uintptr_t)page_object : 0;
        PAGE_MODE = 0;
    }
    if (*m != 16 || m[1] < 0 || m[1] > 15)
        return 0;
    pad = m[1];
    if (page_now == 60 && sim_seq[0] == 6) {
        if (sim_subpad)
            sim_seq[0x5A / 2] = (int16_t)(0x2F + PSTORE(0x2BC) * 16 + pad);
        else if (!sim_patedit)
            step_due_pad = pad;
        return 0;
    }
    /* The pattern screen's pads, as FUN_801350B8 takes them: DELETE flips
       the pattern; COPY and EXCHANGE choose the source, then the
       destination, or COPY flips a sample to keep. */
    if (page_now == 60) {
        int mode = PSTORE(0x34), bank = PSTORE(0x2BC);
        if (mode == 6) {
            PSTORE(0x2F8 + 4 * (bank * 16 + pad)) = PSTORE(0x2F8 + 4 * (bank * 16 + pad)) > 0 ? 0 : 1;
        } else if ((mode == 8 || mode == 10) && PSTORE(0x2B8) < 0) {
            PSTORE(0x2B8) = pad;
            PSTORE(0x2DC) = bank;
            PSTORE(0x2F0) = bank;
        } else if (mode == 8 && sim_keep) {
            int i = PSTORE(0x2F0) * 16 + pad;
            PSTORE(0x57C + 4 * i) = PSTORE(0x57C + 4 * i) == 0;
        } else if (mode == 8 || mode == 10) {
            PSTORE(0x2D8) = pad;
        } else {
            PSTORE(0x2B8) = pad;
        }
        return 0;
    }
    index = sim_bank * 16 + pad;
    if (page_now == 85 && PAGE_MODE == 0)
        store[0x134 + index] = store[0x134 + index] == 0;
    else if (page_now == 85 && PAGE_MODE == 1)
        store[0x1D4 + pad] = store[0x1D4 + pad] == 0;
    else if (page_now == 85 && PAGE_MODE == 2)
        *(int32_t *)(store + 0x1E4) = index;
    else if (page_now == 67 && PAGE_MODE == 1)
        *(int32_t *)(store + 0x200 + 4 * index) = *(int32_t *)(store + 0x200 + 4 * index) > 0 ? 0 : 1;
    return 0;
}
static unsigned sim_drawn;

/* The global drawing context icons land on: the surface first, the origin
   at 0x14 and 0x18. */
static uint32_t icon_ctx[8];

void sim_draw_icon(void *page, int x, int y, int sel, const char *pos, const char *neg)
{
    (void)page; (void)x; (void)y; (void)sel; (void)pos; (void)neg;
}

int sim_draw_string(void *surface, int x, int y, const char *text, int len)
{
    (void)surface; (void)x; (void)y; (void)text; (void)len;
    sim_drawn++;
    return 0;
}

void sim_clear(void *surface, int colour) { (void)surface; (void)colour; }
void sim_fill(void *surface, const int32_t *rect) { (void)surface; (void)rect; }
void sim_frame(void *surface, const int32_t *rect) { (void)surface; (void)rect; }

static int32_t colour_now = 0x1000000;

static int32_t sim_mark(void *surface)
{
    (void)surface;
    return colour_now;
}

static int32_t sim_ink(void *surface)
{
    (void)surface;
    return 0xFFFFFF;
}

static uint32_t surface_vtable[0x140 / 4];
static struct { const uint32_t *vtable; } surfaces[4] = {
    { surface_vtable }, { surface_vtable }, { surface_vtable }, { surface_vtable },
};

static char *slurp(const char *name)
{
    int fd = sh_open(name, 0), n;
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

enum { E_TEXT, E_VALUE, E_CLEAR, E_FILL, E_ICON, E_ROW, E_PAGE, E_KEY, E_KNOB, E_CTRL, E_BOX,
       E_TAB, E_PAD, E_MODE, E_BANK, E_PMODE, E_PKEEP, E_TRREC, E_SCROLL };
#define EVENTS 4096
static struct {
    uint32_t ms;
    int absolute, kind, surf, x, y, x1, y1, done;
    uint32_t site;
    const char *text;
} ev[EVENTS];
static int nev;

static void load_events(void)
{
    char *t = slurp("sim_draws.txt"), *line, *end;

    for (line = t; line && *line; line = end) {
        char *p = line;
        end = strchr(line, '\n');
        if (end)
            *end++ = 0;
        if (*line == 0 || *line == '#' || nev >= EVENTS)
            continue;
        if (*p == '@') {
            ev[nev].absolute = 1;
            p++;
        }
        ev[nev].ms = (uint32_t)strtoul(p, &p, 10);
        while (*p == ' ')
            p++;
        if (p[0] == 's' && p[1] >= '0' && p[1] <= '3' && p[2] == ' ') {
            ev[nev].surf = p[1] - '0';
            p += 3;
        }
        if (strncmp(p, "VALUE", 5) == 0) {
            ev[nev].kind = E_VALUE;
        } else if (strncmp(p, "CLEAR", 5) == 0) {
            ev[nev].kind = E_CLEAR;
        } else if (strncmp(p, "KEY ", 4) == 0) {
            ev[nev].kind = E_KEY;
            ev[nev].x = strncmp(p + 4, "DOWN", 4) == 0 ? 5 : 6;
            ev[nev].y = (int)strtol(p + (ev[nev].x == 5 ? 9 : 7), NULL, 16);
        } else if (strncmp(p, "KNOB", 4) == 0 || strncmp(p, "CTRL", 4) == 0) {
            ev[nev].kind = *p == 'K' ? E_KNOB : E_CTRL;
            ev[nev].x = (int)strtol(p + 4, &p, 10);
            ev[nev].y = (int)strtol(p, &p, 10);
        } else if (strncmp(p, "ROW", 3) == 0) {
            ev[nev].kind = E_ROW;
            p += 3;
            ev[nev].y = (int)strtol(p, &p, 10);
            ev[nev].x1 = (int)strtol(p, &p, 10);
        } else if (strncmp(p, "TAB ", 4) == 0) {
            ev[nev].kind = E_TAB;
            ev[nev].x = (int)strtol(p + 4, &p, 10);
            while (*p == ' ')
                p++;
            ev[nev].text = p;
        } else if (strncmp(p, "SCROLL", 6) == 0) {
            ev[nev].kind = E_SCROLL;
            ev[nev].x1 = (int)strtol(p + 6, &p, 10);
            ev[nev].x = (int)strtol(p, &p, 10);
            ev[nev].y = (int)strtol(p, &p, 10);
            if (*p == ' ')
                p++;
            ev[nev].text = p;
        } else if (strncmp(p, "PAGE", 4) == 0) {
            ev[nev].kind = E_PAGE;
            ev[nev].x = (int)strtol(p + 4, &p, 10);
        } else if (strncmp(p, "TRREC", 5) == 0) {
            ev[nev].kind = E_TRREC;
            ev[nev].x = (int)strtol(p + 5, &p, 10);
        } else if (strncmp(p, "PMODE", 5) == 0 || strncmp(p, "PKEEP", 5) == 0) {
            ev[nev].kind = p[1] == 'M' ? E_PMODE : E_PKEEP;
            ev[nev].x = (int)strtol(p + 5, &p, 10);
        } else if (strncmp(p, "PAD ", 4) == 0 || strncmp(p, "MODE", 4) == 0
                   || strncmp(p, "BANK", 4) == 0) {
            ev[nev].kind = p[1] == 'A' ? (p[2] == 'D' ? E_PAD : E_BANK) : E_MODE;
            ev[nev].x = (int)strtol(p + 4, &p, 10);
        } else if (strncmp(p, "ICON", 4) == 0) {
            ev[nev].kind = E_ICON;
            p += 4;
            ev[nev].x = (int)strtol(p, &p, 10);
            ev[nev].y = (int)strtol(p, &p, 10);
            ev[nev].x1 = (int)strtol(p, &p, 10);
            while (*p == ' ')
                p++;
            ev[nev].text = p;
        } else if (strncmp(p, "FILL", 4) == 0 || strncmp(p, "BOX", 3) == 0) {
            ev[nev].kind = *p == 'F' ? E_FILL : E_BOX;
            p += *p == 'F' ? 4 : 3;
            ev[nev].x = (int)strtol(p, &p, 10);
            ev[nev].y = (int)strtol(p, &p, 10);
            ev[nev].x1 = (int)strtol(p, &p, 10);
            ev[nev].y1 = (int)strtol(p, &p, 10);
            while (*p == ' ')
                p++;
            if (*p == '=')
                ev[nev].site = (uint32_t)strtoul(p + 1, NULL, 16);
        } else {
            ev[nev].kind = E_TEXT;
            ev[nev].x = (int)strtol(p, &p, 10);
            ev[nev].y = (int)strtol(p, &p, 10);
            if (*p == ' ')
                p++;
            ev[nev].text = p;
            for (; *p; p++)
                if (p[0] == '\\' && p[1] == 'n') {
                    p[0] = '\n';
                    memmove(p + 1, p + 2, strlen(p + 2) + 1);
                }
        }
        nev++;
    }
}

/* What the reader said, batch by batch, and what it should have. */
#define BATCHES 256
static char *said[BATCHES];
static int nsaid;

void sim_batch(const char *phrases, int count)
{
    char line[1024];
    size_t n = 0;
    int i;

    if (count == 0 || nsaid >= BATCHES)
        return;
    line[0] = 0;
    for (i = 0; i < count; i++) {
        n += (size_t)snprintf(line + n, sizeof line - n, "%s%s", i ? " | " : "", phrases);
        phrases += strlen(phrases) + 1;
    }
    said[nsaid] = malloc(n + 1);
    memcpy(said[nsaid++], line, n + 1);
}

static void check_expected(void)
{
    char *t = slurp("sim_expect.txt"), *line, *end;
    int k = 0, bad = 0;

    if (t == NULL)
        return;
    for (line = t; *line; line = end) {
        end = strchr(line, '\n');
        if (end)
            *end++ = 0;
        else
            end = line + strlen(line);
        if (*line == 0 || *line == '#')
            continue;
        if (k >= nsaid) {
            printf("EXPECTED batch %d: %s\n     said nothing more\n", k + 1, line);
            bad = 1;
            break;
        }
        if (strcmp(line, said[k]) != 0) {
            printf("EXPECTED batch %d: %s\n     said: %s\n", k + 1, line, said[k]);
            bad = 1;
        }
        k++;
    }
    for (; k < nsaid; k++) {
        printf("UNEXPECTED batch %d: %s\n", k + 1, said[k]);
        bad = 1;
    }
    printf("%s\n", bad ? "FAIL" : "PASS: every batch as expected");
}

/* Virtual time. */
static uint32_t now_ms, ticks, tick_acc, live_ms, live;

uint32_t device_ticks(void)
{
    return ticks;
}

#define RING 16384u
static int16_t ring[RING];
static volatile uint32_t ring_wr, ring_rd;
static uint32_t play_acc, flushes;
/* What was played goes to sim.raw through a fixed buffer, off the heap, which
   holds no more than the instrument's. */
static int16_t played[4096];
static size_t nbuffered, nplayed;
static int raw_fd = -1;

static void played_flush(void)
{
    if (raw_fd >= 0 && nbuffered > 0)
        sh_write(raw_fd, played, (int)(nbuffered * sizeof *played));
    nbuffered = 0;
}

size_t audio_space(void) { return RING - 1u - (ring_wr - ring_rd); }
size_t audio_pending(void) { return ring_wr - ring_rd; }
int audio_hooked(void) { return 1; }

void audio_push(const int16_t *s, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++)
        ring[(ring_wr + i) % RING] = s[i];
    ring_wr += (uint32_t)n;
}

void audio_flush(void)
{
    if (ring_rd != ring_wr)
        flushes++;
    ring_rd = ring_wr;
}

static int held;

uint32_t audio_making(int making)
{
    (void)making;
    return 0;
}

void audio_hold(int hold)
{
    held = hold != 0;
}

static void play_one_ms(void)
{
    if (held)
        return;
    play_acc += 11025;
    while (play_acc >= 1000 && ring_rd < ring_wr) {
        if (nbuffered == sizeof played / sizeof played[0])
            played_flush();
        played[nbuffered++] = ring[ring_rd % RING];
        nplayed++;
        ring_rd++;
        play_acc -= 1000;
    }
    if (ring_rd >= ring_wr)
        play_acc = 0;
}

static void fire(int i)
{
    void *s = &surfaces[ev[i].surf];
    volatile uint32_t *vt = sim_vtables[i % 7];

    ev[i].done = 1;
    switch (ev[i].kind) {
    case E_VALUE:
        printf("sim %lu ms: VALUE, which ends the run\n", (unsigned long)now_ms);
        request_unload();
        break;
    case E_CLEAR:
        ((void (*)(void *, int))vt[0x08 / 4])(s, 0);
        break;
    case E_ROW: {
        int16_t rect[8] = { 0, 0, (int16_t)(ev[i].y - 1), 0, 127, 0, 0, 0 };
        icon_ctx[0] = (uint32_t)(uintptr_t)s;
        ((void (*)(void *, void *, const int16_t *, int, int, int, int))sim_row_word)(
            s, icon_ctx, rect, 0, 0, ev[i].x1, 0);
        break;
    }
    case E_TAB: {
        const char *t = ev[i].text;
        int n = 0, k;
        while (*t && n < 16) {
            for (k = 0; *t && *t != ',' && k < 23; t++)
                tab_names[n][k++] = *t;
            tab_names[n][k] = 0;
            *(const char **)((char *)tab_widget + 0x29C + 4 * n) = tab_names[n];
            n++;
            if (*t == ',')
                t++;
        }
        tab_ctx[0] = (uint32_t)(uintptr_t)s;
        *(uint32_t *)((char *)tab_widget + 0x84) = (uint32_t)(uintptr_t)tab_ctx;
        *(int32_t *)((char *)tab_widget + 0x320) = ev[i].x;
        *(int32_t *)((char *)tab_widget + 0x328) = n;
        ((void (*)(void *))sim_tab_word)(tab_widget);
        break;
    }
    case E_KEY: {
        int16_t m[3] = { (int16_t)ev[i].x, (int16_t)ev[i].y, 0 };
        ((int (*)(void *))sim_page_table[page_now])(m);
        break;
    }
    case E_KNOB:
    case E_CTRL: {
        int16_t m[3] = { ev[i].kind == E_KNOB ? 7 : 9, (int16_t)ev[i].x, (int16_t)ev[i].y };
        ((int (*)(void *))sim_page_table[page_now])(m);
        break;
    }
    case E_PAD: {
        /* The key, then the pad, as the unit sends them; the pad becomes
           the current one. */
        int16_t key[3] = { 5, (int16_t)(ev[i].x - 1), 0 };
        int16_t pad[3] = { 16, (int16_t)(ev[i].x - 1), 100 };
        sim_padnow = ev[i].x - 1;
        ((int (*)(void *))sim_page_table[page_now])(key);
        ((int (*)(void *))sim_page_table[page_now])(pad);
        break;
    }
    case E_MODE:
        PAGE_MODE = ev[i].x;
        break;
    case E_BANK:
        sim_bank = ev[i].x;
        break;
    case E_PMODE:
        /* Each operation starts with nothing chosen, as FUN_800E11D0 sets
           the mode. */
        PSTORE(0x34) = ev[i].x;
        PSTORE(0x2B8) = PSTORE(0x2D8) = PSTORE(0x2E4) = PSTORE(0x2E8) = -1;
        PSTORE(0x2EC) = 0;
        memset((char *)sim_patstore + 0x2F8, 0, 160 * 4);
        break;
    case E_PKEEP:
        sim_keep = ev[i].x;
        break;
    case E_TRREC:
        sim_seq_state = (uint32_t)(uintptr_t)sim_seq;
        sim_seq[0] = ev[i].x ? 6 : 1;
        if (ev[i].x)
            sim_seq[0x5A / 2] = (int16_t)ev[i].x;
        break;
    case E_PAGE: {
        /* Building the page, then the page's steady traffic, which must
           not count. */
        int16_t build = 1, tick = 3;
        page_now = ev[i].x % 94;
        ((int (*)(void *))sim_page_table[ev[i].x % 94])(&build);
        ((int (*)(void *))sim_page_table[ev[i].x % 94])(&tick);
        ((int (*)(void *))sim_page_table[ev[i].x % 94])(&tick);
        break;
    }
    case E_ICON:
        icon_ctx[0] = (uint32_t)(uintptr_t)s;
        ((void (*)(void *, int, int, int, const char *, const char *))sim_icon_slots[i % 2])(
            s, ev[i].x, ev[i].y, ev[i].x1, ev[i].text, ev[i].text);
        break;
    case E_FILL: {
        int32_t r[4] = { ev[i].x, ev[i].y, ev[i].x1, ev[i].y1 };
        ((void (*)(void *, const int32_t *))vt[0xC0 / 4])(s, r);
        break;
    }
    case E_BOX: {
        int32_t r[4] = { ev[i].x, ev[i].y, ev[i].x1, ev[i].y1 };
        sim_site = ev[i].site;
        ((void (*)(void *, const int32_t *))vt[0xBC / 4])(s, r);
        break;
    }
    default: {
        const char *t = ev[i].text;
        colour_now = 0x1000000;
        sim_site = 0;
        for (;; t++) {
            if (*t == '!')
                colour_now = 0xFFFFFF;
            else if (*t == '~')
                colour_now = -1;
            else if (*t == '_')
                colour_now = 0;
            else if (*t == '^')
                sim_site = TITLE_SITE;
            else if (*t == '=' && strlen(t) > 9 && t[9] == ':') {
                sim_site = (uint32_t)strtoul(t + 1, NULL, 16);
                t += 9;
            } else
                break;
        }
        if (ev[i].kind == E_SCROLL) {
            char *whole = (char *)scroll_owner + 0x35C;
            snprintf(whole, 256, "%s", t);
            scroll_owner[0x1A8C / 4] = (uint32_t)ev[i].x1;
            sim_scroll_owner = (uint32_t)(uintptr_t)scroll_owner;
            t = whole + ev[i].x1;
        }
        ((int (*)(void *, int, int, const char *, int))vt[0x12C / 4])(
            s, ev[i].x, ev[i].y, t, (int)strlen(t));
    }
    }
}

void target_sleep(int ms)
{
    int k, i;

    if (ms <= 0)
        ms = 1;
    for (k = 0; k < ms; k++) {
        now_ms++;
        tick_acc += 3;
        ticks += tick_acc / 4;
        tick_acc %= 4;
        play_one_ms();
        if (step_due_pad >= 0 && step_due_ms == 0)
            step_due_ms = now_ms + 60;
        if (step_due_pad >= 0 && now_ms >= step_due_ms) {
            int note = sim_seq[0x5A / 2], high = note > 0x7E;
            volatile uint16_t *w = &sim_step_bits[step_due_pad * 4 * 128 + (high ? note - 0x50 : note)];
            *w ^= (uint16_t)(1u << high);
            step_due_pad = -1;
            step_due_ms = 0;
        }
        if (!live && sim_vtables[0][0x12C / 4] != (uint32_t)(uintptr_t)sim_draw_string) {
            live = 1;
            live_ms = now_ms;
            printf("sim %lu ms: screen hook live\n", (unsigned long)now_ms);
        }
        for (i = 0; i < nev; i++) {
            if (ev[i].done)
                continue;
            if (ev[i].absolute ? now_ms >= ev[i].ms : live && now_ms - live_ms >= ev[i].ms)
                fire(i);
        }
    }
    if (now_ms > 600000u) {
        printf("sim: ten minutes of virtual time, giving up\n");
        exit(5);
    }
}

void target_wake(void) { }
void engine_install(void) { }
void target_checkpoint(void) { }
void target_volume(uint32_t percent) { (void)percent; }
void target_probe_slots(void) { }
void target_probe_rx(void) { }
void target_output(int word) { (void)word; }
int engine_task_id(void) { return 1; }
int kernel_task_self(void) { return 1; }

void write_file(const char *path, const void *buf, size_t len)
{
    char name[64];
    const char *base = strrchr(path, '/');
    int fd;

    snprintf(name, sizeof name, "sim_%s", base ? base + 1 : path);
    fd = sh_open(name, 4 | 1);
    if (fd < 0)
        return;
    sh_write(fd, buf, (int)len);
    sh_close(fd);
}

int file_exists(const char *path)
{
    char name[64];
    const char *base = strrchr(path, '/');
    int fd;

    snprintf(name, sizeof name, "sim_%s", base ? base + 1 : path);
    fd = sh_open(name, 0);
    if (fd < 0)
        return 0;
    sh_close(fd);
    return 1;
}

const char *read_text(const char *path)
{
    if (strstr(path, "TALLFREE.CFG") != NULL)
        return slurp("sim_TALLFREE.CFG");
    return slurp("sim_say.txt");
}

void target_done(int rc)
{
    screen_remove();
    printf("engine finished, code %d\n", rc);
    screen_log_write();
}

void target_enter(void)
{
    unsigned i;

    for (i = 0; i < 7; i++) {
        sim_vtables[i][0x12C / 4] = (uint32_t)(uintptr_t)sim_draw_string;
        sim_vtables[i][0x08 / 4] = (uint32_t)(uintptr_t)sim_clear;
        sim_vtables[i][0xC0 / 4] = (uint32_t)(uintptr_t)sim_fill;
        sim_vtables[i][0xBC / 4] = (uint32_t)(uintptr_t)sim_frame;
    }
    surface_vtable[0x18 / 4] = (uint32_t)(uintptr_t)sim_mark;
    surface_vtable[0x10 / 4] = (uint32_t)(uintptr_t)sim_ink;
    sim_icon_slots[0] = sim_icon_slots[1] = (uint32_t)(uintptr_t)sim_draw_icon;
    sim_icon_ctx = (uint32_t)(uintptr_t)icon_ctx;
    sim_row_word = (uint32_t)(uintptr_t)sim_draw_row;
    sim_tab_word = (uint32_t)(uintptr_t)sim_draw_tabs;
    for (i = 0; i < 94; i++)
        sim_page_table[i] = (uint32_t)(uintptr_t)sim_page_factory;
    load_events();
    raw_fd = sh_open("sim.raw", 4 | 1);
}

void target_leave(void) { }

int target_launch(int (*body)(void))
{
    int rc = body(), i, restored = 1;

    for (i = 0; i < 7; i++)
        if (sim_vtables[i][0x12C / 4] != (uint32_t)(uintptr_t)sim_draw_string
            || sim_vtables[i][0x08 / 4] != (uint32_t)(uintptr_t)sim_clear
            || sim_vtables[i][0xC0 / 4] != (uint32_t)(uintptr_t)sim_fill
            || sim_vtables[i][0xBC / 4] != (uint32_t)(uintptr_t)sim_frame)
            restored = 0;
    printf("sim %lu ms: done, code %d; tables %s, %u draws reached DrawString, "
           "%u keys reached the page, %lu flushes, %lu samples played\n",
           (unsigned long)now_ms, rc, restored ? "restored" : "NOT RESTORED",
           sim_drawn, page_saw_keys, (unsigned long)flushes, (unsigned long)nplayed);
    for (i = 0; i < nsaid; i++)
        printf("said %d: %s\n", i + 1, said[i]);
    check_expected();
    played_flush();
    if (raw_fd >= 0)
        sh_close(raw_fd);
    return rc;
}
