/* The resident service under QEMU, with the instrument simulated round it.

   Time is virtual and moves only when the service sleeps, 0.75 audio ticks
   and 11.025 samples of playback to the millisecond. Draws and VALUE presses
   come from sim_draws.txt at their times, through the same table words the
   instrument's interface would call through, so the hook, the model, the
   cut-offs and the unload path all run as they would on the unit. Settings
   and the script come from sim_say.txt, files the service writes land beside
   the run as sim_*.TXT, and what was played goes to sim.raw.

   sim_draws.txt, one event a line:
     MS X Y TEXT   drawn MS ms after screen reading goes live
     MS VALUE      VALUE pressed then
     @MS VALUE     VALUE pressed MS ms after the start */

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

volatile uint32_t sim_vtable[7], sim_key_word = 0x60308001u;
static unsigned sim_drawn, sim_loader_presses;

int sim_draw_string(void *surface, int x, int y, const char *text, int len)
{
    (void)surface;
    (void)x;
    (void)y;
    (void)text;
    (void)len;
    sim_drawn++;
    return 0;
}

static int32_t sim_mark(void *surface)
{
    (void)surface;
    return 7;
}

static uint32_t surface_vtable[0x140 / 4];
static struct { const uint32_t *vtable; } surface = { surface_vtable };

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

#define EVENTS 256
static struct { uint32_t ms; int absolute, key, x, y; const char *text; int done; } ev[EVENTS];
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
        if (strncmp(p, "VALUE", 5) == 0) {
            ev[nev].key = 1;
        } else {
            ev[nev].x = (int)strtol(p, &p, 10);
            ev[nev].y = (int)strtol(p, &p, 10);
            if (*p == ' ')
                p++;
            ev[nev].text = p;
        }
        nev++;
    }
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
    ev[i].done = 1;
    if (ev[i].key) {
        printf("sim %lu ms: VALUE\n", (unsigned long)now_ms);
        if (sim_key_word == 0x60308001u) {
            printf("sim: VALUE went to the loader, which would load over a running engine\n");
            sim_loader_presses++;
        } else {
            ((int (*)(void *, int))sim_key_word)(NULL, 0x31);
        }
        return;
    }
    ((int (*)(void *, int, int, const char *, int))sim_vtable[i % 7])(
        &surface, ev[i].x, ev[i].y, ev[i].text, (int)strlen(ev[i].text) + 4);
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
        if (!live && sim_vtable[0] != (uint32_t)(uintptr_t)sim_draw_string) {
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
void target_checkpoint(void) { printf("sim %lu ms: checkpoint\n", (unsigned long)now_ms); }
void target_volume(uint32_t percent) { printf("volume %lu%%\n", (unsigned long)percent); }
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

    for (i = 0; i < 7; i++)
        sim_vtable[i] = (uint32_t)(uintptr_t)sim_draw_string;
    surface_vtable[0x18 / 4] = (uint32_t)(uintptr_t)sim_mark;
    load_events();
}

void target_leave(void) { }

int target_launch(int (*body)(void))
{
    int rc = body(), i, fd, restored = 1;

    key_remove();
    for (i = 0; i < 7; i++)
        if (sim_vtable[i] != (uint32_t)(uintptr_t)sim_draw_string)
            restored = 0;
    printf("sim %lu ms: done, code %d; tables %s, key %s, %u draws reached DrawString, "
           "%u presses went to the loader, %lu flushes, %lu samples played\n",
           (unsigned long)now_ms, rc, restored ? "restored" : "NOT RESTORED",
           sim_key_word == 0x60308001u ? "back to the loader" : "NOT RESTORED",
           sim_drawn, sim_loader_presses, (unsigned long)flushes, (unsigned long)nplayed);
    fd = sh_open("sim.raw", 4 | 1);
    if (fd >= 0) {
        sh_write(fd, played, (int)(nplayed * sizeof *played));
        sh_close(fd);
    }
    return rc;
}
