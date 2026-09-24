/* The resident engine on the instrument: the boot loader starts it, and it
   says "speech on", speaks any script in A:/EVV/SAY.TXT, reads the screen
   from then until the instrument is switched off, and copies itself to the
   eMMC if that has an older copy or none.

   SAY.TXT holds settings and a script, one per line:
     #vol N      speech level, percent; 50 by default
     #mode M     screen reading: changed (the default), all, or off
     #settle N   ms without a change before speaking what changed; 40
     #frame N    samples per synthesised buffer; 512
     #wait N     the next line is said N ms after the one before it started,
                 cutting that one off; without it, a line waits for the one
                 before to finish
     #slots      the slot probe instead of any of this
   and any other line is said.

   Speech is a batch of phrases said in turn. A new batch, from the screen or
   the script, cuts off the old one: what is queued to play is dropped at
   once, and the rest of the phrase being synthesised is thrown away as it
   arrives, since the engine cannot abandon an utterance it has begun. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "device.h"
#include "say.h"

static int mode = SCREEN_CHANGED, settle_ms = 40, frame = 512, slots;

#define SCRIPT 64
static struct { const char *text; int wait_ms; } script[SCRIPT];
static int nscript, script_next;
static uint32_t last_submit;

static char batch[4096];
static size_t batch_pos;
static int batch_left;

static int synth_active, discarding, unloading, screen_live, screen_started;
static uint32_t t_submit, t_first, phrase_no;
static size_t phrase_samples;
static char phrase[48];

static void configure(char *text)
{
    int wait_ms = -1;
    char *line = text, *end;

    for (; line && *line; line = end) {
        end = strchr(line, '\n');
        if (end)
            *end++ = 0;
        if (*line && line[strlen(line) - 1] == '\r')
            line[strlen(line) - 1] = 0;
        if (strncmp(line, "#vol ", 5) == 0)
            target_volume((uint32_t)strtoul(line + 5, NULL, 10));
        else if (strncmp(line, "#mode ", 6) == 0)
            mode = strcmp(line + 6, "off") == 0 ? SCREEN_OFF
                 : strcmp(line + 6, "all") == 0 ? SCREEN_ALL : SCREEN_CHANGED;
        else if (strncmp(line, "#settle ", 8) == 0)
            settle_ms = atoi(line + 8);
        else if (strncmp(line, "#frame ", 7) == 0)
            frame = atoi(line + 7);
        else if (strncmp(line, "#wait ", 6) == 0)
            wait_ms = atoi(line + 6);
        else if (strncmp(line, "#slots", 6) == 0)
            slots = 1;
        else if (*line && *line != '#' && nscript < SCRIPT) {
            script[nscript].text = line;
            script[nscript].wait_ms = wait_ms;
            nscript++;
            wait_ms = -1;
        }
    }
    printf("config: mode %d, settle %d ms, frame %d, %d script lines\n",
           mode, settle_ms, frame, nscript);
}

static void batch_set(const char *phrases, int count)
{
    const char *p = phrases;
    size_t len = 0;
    int i;

    for (i = 0; i < count; i++) {
        size_t n = strlen(p) + 1;
        if (len + n > sizeof batch)
            break;
        len += n;
        p += n;
    }
    memcpy(batch, phrases, len);
    batch_pos = 0;
    batch_left = i;
    last_submit = device_ticks();
}

static void batch_one(const char *text)
{
    batch_set(text, 1);
}

static void cut(void)
{
    audio_flush();
    if (synth_active)
        discarding = 1;
    batch_left = 0;
}

static int idle(void)
{
    return !synth_active && batch_left == 0 && audio_pending() == 0;
}

/* Everything that can change what is to be said: VALUE, the screen and the
   script. Called from the main loop and from inside a wait for room in the
   ring, so a long phrase can be cut off while it plays. */
static void service_poll(void)
{
    static char phrases[4096];
    int count;

    if (unload_requested() && !unloading) {
        unloading = 1;
        screen_remove();
        screen_live = 0;
        cut();
        batch_one("speech off");
        return;
    }
    if (unloading)
        return;
    if (screen_live) {
        if (screen_poll(phrases, sizeof phrases, &count)) {
            cut();
            batch_set(phrases, count);
        }
        return;
    }
    if (script_next < nscript) {
        int w = script[script_next].wait_ms;
        if ((w < 0 && idle())
            || (w >= 0 && device_ticks() - last_submit >= MS_TO_TICKS(w))) {
            if (w >= 0)
                cut();
            batch_one(script[script_next].text);
            script_next++;
        }
    }
}

void target_audio(const short *s, size_t n)
{
    if (discarding || !audio_hooked())
        return;
    if (phrase_samples == 0)
        t_first = device_ticks();
    phrase_samples += n;
    while (audio_space() < n) {
        service_poll();
        if (discarding)
            return;
        target_sleep(5);
    }
    audio_push(s, n);
}

static void phrase_start(void)
{
    const char *p = batch + batch_pos;

    batch_pos += strlen(p) + 1;
    batch_left--;
    strncpy(phrase, p, sizeof phrase - 1);
    phrase[sizeof phrase - 1] = 0;
    phrase_no++;
    phrase_samples = 0;
    discarding = 0;
    t_submit = device_ticks();
    t_first = t_submit;
    if (speech_say(p) == 0)
        synth_active = 1;
}

/* Times in ms from the phrase being handed over: to its first buffer, and
   to the end of its synthesis. */
static void phrase_done(void)
{
    uint32_t now = device_ticks();

    printf("say %lu at %lu: first %lu ms, all %lu ms, %lu samples%s |%s|\n",
           (unsigned long)phrase_no, (unsigned long)t_submit,
           (unsigned long)(phrase_samples ? TICKS_TO_MS(t_first - t_submit) : 0),
           (unsigned long)TICKS_TO_MS(now - t_submit), (unsigned long)phrase_samples,
           discarding ? ", cut off" : "", phrase);
    synth_active = 0;
    discarding = 0;
}

/* Once the script has been said: the log so far to the card, in case the
   screen hook turns out to hang the instrument, and then the hook. */
static void start_screen(void)
{
    screen_started = 1;
    target_checkpoint();
    if (mode == SCREEN_OFF)
        return;
    if (screen_install(mode, settle_ms) == 0)
        screen_live = 1;
    else
        batch_one("no screen hook");
}

int target_main(void)
{
    const char *text = read_text("A:/EVV/SAY.TXT");
    static char config[4096];
    uint32_t last_flush = 0, started = device_ticks();
    int installed = 0;

    if (text != NULL) {
        strncpy(config, text, sizeof config - 1);
        configure(config);
    }
    if (slots) {
        target_probe_slots();
        return 0;
    }
    if (!audio_hooked())
        printf("audio hook not installed\n");
    if (speech_open(frame))
        return 1;
    batch_one("speech on");

    for (;;) {
        service_poll();
        if (synth_active) {
            if (!speech_busy())
                phrase_done();
            continue;
        }
        if (batch_left > 0) {
            phrase_start();
            continue;
        }
        if (unloading) {
            if (audio_pending() == 0 || device_ticks() - last_submit > 5u * 750u)
                break;
        } else if (!screen_started && script_next >= nscript && idle()) {
            start_screen();
            continue;
        }
        /* Five seconds in, with the start-up quiet, once. */
        if (!installed && screen_started && idle() && device_ticks() - started >= 3750u) {
            installed = 1;
            engine_install();
        }
        /* The draw log goes to the card every ten seconds while nothing is
           being said, so a hang still leaves most of it behind. */
        if (screen_live && idle() && device_ticks() - last_flush >= 7500u) {
            last_flush = device_ticks();
            if (screen_log_write())
                target_checkpoint();
        }
        target_sleep(10);
    }
    speech_close();
    screen_report();
    return 0;
}
