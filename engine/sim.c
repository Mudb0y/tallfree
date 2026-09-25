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
     MS [sN] ICON X Y SEL NAME an icon menu's icon, selected if SEL is 1
     MS [sN] ROW Y SEL         a settings row whose text is at Y, selected if 1
     MS PAGE N                 page factory N called: a page being built
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
volatile uint32_t sim_page_table[94], sim_row_word;

void sim_draw_row(void *page, void *ctx, const int16_t *rect, int a, int b, int sel, int c)
{
    (void)page; (void)ctx; (void)rect; (void)a; (void)b; (void)sel; (void)c;
}

static int sim_page_factory(void *request)
{
    (void)request;
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

enum { E_TEXT, E_VALUE, E_CLEAR, E_FILL, E_ICON, E_ROW, E_PAGE };
#define EVENTS 4096
static struct {
    uint32_t ms;
    int absolute, kind, surf, x, y, x1, y1, done;
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
        } else if (strncmp(p, "ROW", 3) == 0) {
            ev[nev].kind = E_ROW;
            p += 3;
            ev[nev].y = (int)strtol(p, &p, 10);
            ev[nev].x1 = (int)strtol(p, &p, 10);
        } else if (strncmp(p, "PAGE", 4) == 0) {
            ev[nev].kind = E_PAGE;
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
        } else if (strncmp(p, "FILL", 4) == 0) {
            ev[nev].kind = E_FILL;
            p += 4;
            ev[nev].x = (int)strtol(p, &p, 10);
            ev[nev].y = (int)strtol(p, &p, 10);
            ev[nev].x1 = (int)strtol(p, &p, 10);
            ev[nev].y1 = (int)strtol(p, &p, 10);
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
static int16_t *played;
static size_t nplayed, played_cap;

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

static void play_one_ms(void)
{
    play_acc += 11025;
    while (play_acc >= 1000 && ring_rd < ring_wr) {
        if (nplayed == played_cap) {
            played_cap = played_cap * 2 + 65536;
            played = realloc(played, played_cap * sizeof *played);
        }
        played[nplayed++] = ring[ring_rd % RING];
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
    case E_PAGE: {
        /* Building the page, then the page's steady traffic, which must
           not count. */
        int16_t build = 1, tick = 3;
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

const char *read_text(const char *path)
{
    (void)path;
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
    }
    surface_vtable[0x18 / 4] = (uint32_t)(uintptr_t)sim_mark;
    surface_vtable[0x10 / 4] = (uint32_t)(uintptr_t)sim_ink;
    sim_icon_slots[0] = sim_icon_slots[1] = (uint32_t)(uintptr_t)sim_draw_icon;
    sim_icon_ctx = (uint32_t)(uintptr_t)icon_ctx;
    sim_row_word = (uint32_t)(uintptr_t)sim_draw_row;
    for (i = 0; i < 94; i++)
        sim_page_table[i] = (uint32_t)(uintptr_t)sim_page_factory;
    load_events();
}

void target_leave(void) { }

int target_launch(int (*body)(void))
{
    int rc = body(), i, fd, restored = 1;

    for (i = 0; i < 7; i++)
        if (sim_vtables[i][0x12C / 4] != (uint32_t)(uintptr_t)sim_draw_string
            || sim_vtables[i][0x08 / 4] != (uint32_t)(uintptr_t)sim_clear
            || sim_vtables[i][0xC0 / 4] != (uint32_t)(uintptr_t)sim_fill)
            restored = 0;
    printf("sim %lu ms: done, code %d; tables %s, %u draws reached DrawString, "
           "%lu flushes, %lu samples played\n",
           (unsigned long)now_ms, rc, restored ? "restored" : "NOT RESTORED",
           sim_drawn, (unsigned long)flushes, (unsigned long)nplayed);
    for (i = 0; i < nsaid; i++)
        printf("said %d: %s\n", i + 1, said[i]);
    check_expected();
    fd = sh_open("sim.raw", 4 | 1);
    if (fd >= 0) {
        sh_write(fd, played, (int)(nplayed * sizeof *played));
        sh_close(fd);
    }
    return rc;
}
