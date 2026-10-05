/* The resident engine on the instrument: the boot loader starts it, and it
   says "ready", speaks any script in A:/TALLFREE/TALLFREE.DEBUG, reads
   the screen from then until the instrument is switched off, and copies
   itself to the eMMC if that has an older copy or none.

   TALLFREE.DEBUG holds settings and a script, one per line:
     #vol N      speech level, percent; 50 by default
     #mode M     screen reading: changed (the default), all, or off
     #settle N   ms without a change before speaking what changed; 40
     #frame N    samples per synthesised buffer; 512
     #dict on    the engine's abbreviation dictionary, off by default: with
                 it, SD-CARD is "South Dakota card"
     #log on     the log, the draw log and any fault's record to the card,
                 off by default
     #wait N     the next line is said N ms after the one before it started,
                 cutting that one off; without it, a line waits for the one
                 before to finish
     #slots      the slot probe instead of any of this
     #rxprobe    the receive probe instead: which output slots come back in
                 from the hardware, to A:/TALLFREE/RXPROBE.TXT
     #out N      speech into line 3 words 14 and 15 (the default), word 12,
                 the metronome's, or 0, words 0 and 1, which recordings
                 catch; among the script's lines, from the next line on, and
                 after the last, once the script is said
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

static int mode = SCREEN_CHANGED, settle_ms = 40, frame = 512, slots, rxprobe, dictionary,
    volume = 50;
int card_log;

/* The card can be swapped while the instrument runs, for samples on
   another, so the log goes only to the card it began on, known by its
   TALLFREE.DEBUG as read at power-on, and only while Roland's flag says a
   card is in. A write cut short by the card coming out may leave the card's
   FAT inconsistent, which hangs the unit's file system on the next write,
   so after one nothing more is written to any card this session, and that
   card is said to need checking when it comes back. */
#define DEBUG_FILE "A:/TALLFREE/TALLFREE.DEBUG"
static uint32_t card_hash, card_in_at;
static int card_was_in = 1, card_seen = 1, card_settling, card_spoiled, card_spoken;

static uint32_t text_hash(const char *t)
{
    uint32_t h = 2166136261u;

    while (*t)
        h = (h ^ (uint8_t)*t++) * 16777619u;
    return h;
}

static int card_is_ours(void)
{
    const char *t;

    if (!card_inserted() || card_settling)
        return 0;
    t = read_text(DEBUG_FILE);
    return t != NULL && text_hash(t) == card_hash;
}

int log_card(void)
{
    return !card_spoiled && card_is_ours();
}

static int card_write(const char *path, const void *buf, size_t len)
{
    int r = write_file(path, buf, len);

    if (r < 0 || (r > 0 && !card_inserted())) {
        card_spoiled = 1;
        return 0;
    }
    return r > 0;
}

/* A log goes to the card in parts, A:/TALLFREE/STEM000.TXT on, so that it
   runs as long as the instrument does: the part being written is rewritten
   whole as it grows, and left once full. The first write of a session
   empties the parts after the first that a longer session left. Only the
   file calls the firmware itself makes: create-and-truncate, write, close,
   and open to read. Whether it was written, so that a part is not left
   while the card is out. */
int log_part_write(const char *stem, int part, int *emptied, const void *buf, size_t len)
{
    char path[40];
    int k;

    if (!log_card())
        return 0;
    if (!*emptied) {
        *emptied = 1;
        for (k = part + 1; k < 1000; k++) {
            snprintf(path, sizeof path, "A:/TALLFREE/%s%03d.TXT", stem, k);
            if (!file_exists(path))
                break;
            if (!card_write(path, "", 0))
                return 0;
        }
    }
    snprintf(path, sizeof path, "A:/TALLFREE/%s%03d.TXT", stem, part);
    return card_write(path, buf, len);
}

#define SCRIPT 64
static struct { const char *text; int wait_ms, out; } script[SCRIPT];
static int nscript, script_next, final_out = -1;
static uint32_t last_submit;

static char batch[4096];
static size_t batch_pos;
static int batch_left;

static int synth_active, discarding, unloading, screen_live, screen_started, holding;
static uint32_t t_submit, t_first, t_released, phrase_no;
static size_t phrase_samples;
static char phrase[48];

static void configure(char *text)
{
    int wait_ms = -1, out = -1;
    char *line = text, *end;

    for (; line && *line; line = end) {
        end = strchr(line, '\n');
        if (end)
            *end++ = 0;
        if (*line && line[strlen(line) - 1] == '\r')
            line[strlen(line) - 1] = 0;
        if (strncmp(line, "#vol ", 5) == 0)
            volume = atoi(line + 5);
        else if (strncmp(line, "#mode ", 6) == 0)
            mode = strcmp(line + 6, "off") == 0 ? SCREEN_OFF
                 : strcmp(line + 6, "all") == 0 ? SCREEN_ALL : SCREEN_CHANGED;
        else if (strncmp(line, "#settle ", 8) == 0)
            settle_ms = atoi(line + 8);
        else if (strncmp(line, "#frame ", 7) == 0)
            frame = atoi(line + 7);
        else if (strcmp(line, "#dict on") == 0)
            dictionary = 1;
        else if (strcmp(line, "#log on") == 0)
            card_log = 1;
        else if (strncmp(line, "#wait ", 6) == 0)
            wait_ms = atoi(line + 6);
        else if (strncmp(line, "#slots", 6) == 0)
            slots = 1;
        else if (strncmp(line, "#rxprobe", 8) == 0)
            rxprobe = 1;
        else if (strncmp(line, "#out ", 5) == 0 && nscript == 0)
            target_output(atoi(line + 5));
        else if (strncmp(line, "#out ", 5) == 0)
            out = atoi(line + 5);
        else if (*line && *line != '#' && nscript < SCRIPT) {
            script[nscript].text = line;
            script[nscript].wait_ms = wait_ms;
            script[nscript].out = out;
            nscript++;
            wait_ms = -1;
            out = -1;
        }
    }
    final_out = out;
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

#ifdef SIM
void sim_own(const char *text);
#endif

static void batch_one(const char *text)
{
#ifdef SIM
    sim_own(text);
#endif
    batch_set(text, 1);
}

static void cut(void)
{
    holding = 0;
    audio_hold(0);
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
            if (script[script_next].out >= 0)
                target_output(script[script_next].out);
            batch_one(script[script_next].text);
            script_next++;
        }
    }
}

/* A phrase starting into silence is held back until it can play straight
   through: until its synthesis is done, or 150 ms of it are waiting and it
   is being made at least 1.2 times as fast as it plays. The engine's cost is
   mostly fixed per phrase, so at a fast speed a short phrase is shorter than
   the time it takes to make, and played as it came it stalled mid-word. */

static void release_when_ahead(void)
{
    uint32_t made_ms, since_ms;

    if (!holding)
        return;
    made_ms = (uint32_t)(phrase_samples * 1000u / 11025u);
    since_ms = TICKS_TO_MS(device_ticks() - t_first);
    if (audio_pending() * 1000u / 11025u >= 150u && since_ms > 0 && made_ms * 10u >= since_ms * 12u) {
        holding = 0;
        audio_hold(0);
        t_released = device_ticks();
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
        holding = 0;
        audio_hold(0);
        service_poll();
        if (discarding)
            return;
        target_sleep(5);
    }
    audio_push(s, n);
    release_when_ahead();
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
    t_released = 0;
    if (audio_pending() == 0) {
        holding = 1;
        audio_hold(1);
    }
    audio_making(1);
    if (speech_say(p) == 0)
        synth_active = 1;
}

/* Times in ms from the phrase being handed over: to its first buffer, and
   to the end of its synthesis. */
static void phrase_done(void)
{
    uint32_t now = device_ticks(), dry = audio_making(0);

    /* played: from handing it over to its sound starting, the first buffer
       or the release from holding; dry: how long it stalled after that. */
    printf("say %lu at %lu: first %lu ms, all %lu ms, played %lu ms, dry %lu ms, %lu samples%s |%s|\n",
           (unsigned long)phrase_no, (unsigned long)t_submit,
           (unsigned long)(phrase_samples ? TICKS_TO_MS(t_first - t_submit) : 0),
           (unsigned long)TICKS_TO_MS(now - t_submit),
           (unsigned long)TICKS_TO_MS((t_released ? t_released : holding ? now : t_first) - t_submit),
           (unsigned long)TICKS_TO_MS(dry), (unsigned long)phrase_samples,
           discarding ? ", cut off" : "", phrase);
    synth_active = 0;
    discarding = 0;
    holding = 0;
    audio_hold(0);
}

/* Polled from the main loop: the card going out and coming in, noted with
   the time. A card just in is left alone for two seconds, for the file
   system to take it up. */
static void card_watch(void)
{
    int in = card_inserted();

    if (in != card_was_in) {
        card_was_in = in;
        card_settling = in;
        card_in_at = device_ticks();
        screen_log_note(in ? "a card in" : "the card out");
    }
    if (card_settling && device_ticks() - card_in_at >= 1500u)
        card_settling = 0;
}

/* Before the logs go out: which card is in, noted when that changes, and a
   card that a cut write may have spoiled said to need checking once it is
   back. */
static void card_look(void)
{
    int now;

    if (card_settling)
        return;
    now = !card_inserted() ? 0 : card_is_ours() ? 1 : 2;
    if (now != card_seen && now != 0)
        screen_log_note(now == 1 ? "the log's card back" : "another card, nothing written to it");
    card_seen = now;
    if (now == 1 && card_spoiled && !card_spoken) {
        card_spoken = 1;
        batch_one("log card needs checking");
    }
}

/* Once the script has been said: the log so far to the card, in case the
   screen hook turns out to hang the instrument, and then the hook. */
static void start_screen(void)
{
    screen_started = 1;
    if (final_out >= 0)
        target_output(final_out);
    if (card_log)
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
    const char *text = read_text("A:/TALLFREE/TALLFREE.DEBUG");
    static char config[4096];
    uint32_t last_flush = 0, started = device_ticks();
    int installed = 0;

    if (text != NULL) {
        card_hash = text_hash(text);
        strncpy(config, text, sizeof config - 1);
        configure(config);
    }
    if (slots) {
        target_probe_slots();
        return 0;
    }
    if (rxprobe) {
        target_probe_rx();
        return 0;
    }
    if (!audio_hooked())
        printf("audio hook not installed\n");
    if (speech_open(frame))
        return 1;
    /* The saved speech settings over TALLFREE.DEBUG's: speed, pitch, voice,
       volume, the abbreviation dictionary and screen reading. */
    settings_load(volume, dictionary);
    batch_one("ready");

    for (;;) {
        service_poll();
        if (card_log)
            card_watch();
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
        if (card_log && screen_live && idle() && device_ticks() - last_flush >= 7500u) {
            last_flush = device_ticks();
            card_look();
            if (screen_log_write())
                target_checkpoint();
        }
        target_sleep(10);
    }
    speech_close();
    screen_report();
    return 0;
}
