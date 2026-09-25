/* The speech settings: speed, pitch, voice, volume, abbreviations and screen
   reading, kept on the eMMC as B:/TALLFREE.CFG.

   SHIFT + EXIT opens the menu from any screen; the screen hook keeps EXIT
   and the VALUE knob and its press for it while it is open, and hands
   them here. Turning VALUE moves between settings; a press starts changing
   the one it is on, turning then changes it, spoken in the new setting at
   once, and another press goes back to choosing. EXIT saves and closes. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "device.h"
#include "say.h"

int screen_say_text(const char *text);

enum { SPEED, PITCH, VOICE, VOLUME, ABBREVIATIONS, READING, SETTINGS };

static const char *const voices[8] = {
    "Adult Male 1", "Adult Female 1", "Child 1", "Adult Male 2",
    "Adult Male 3", "Adult Female 2", "Elderly Female 1", "Elderly Male 1",
};

static struct {
    const char *name, *key;
    int min, max, step, value;
} settings[SETTINGS] = {
    { "Speed", "speed", 0, 250, 5, 50 },
    { "Pitch", "pitch", 0, 100, 5, 65 },
    { "Voice", "voice", 1, 8, 1, 1 },
    /* Never silent: a menu turned down to nothing could not be found again. */
    { "Volume", "volume", 10, 100, 5, 50 },
    { "Abbreviations", "abbreviations", 0, 1, 1, 0 },
    { "Screen reading", "reading", 0, 1, 1, 1 },
};

static int current, editing, open_now;

/* eci.h's numbers: pitch baseline 2, speed 6; eciDictionary 3, inverted. */
static void apply(int which)
{
    switch (which) {
    case SPEED:
        speech_voice_set(6, settings[SPEED].value);
        break;
    case PITCH:
        speech_voice_set(2, settings[PITCH].value);
        break;
    case VOICE:
        /* A preset brings its own pitch; the speed stays the reader's. */
        speech_voice(settings[VOICE].value);
        settings[PITCH].value = speech_voice_get(2);
        speech_voice_set(6, settings[SPEED].value);
        break;
    case VOLUME:
        target_volume((uint32_t)settings[VOLUME].value);
        break;
    case ABBREVIATIONS:
        speech_param(3, settings[ABBREVIATIONS].value ? 0 : 1);
        break;
    case READING:
        screen_mute(!settings[READING].value);
        break;
    }
}

static void value_text(int which, char *out, size_t cap)
{
    int v = settings[which].value;

    if (which == VOICE)
        snprintf(out, cap, "%s", voices[(v - 1) & 7]);
    else if (which == ABBREVIATIONS || which == READING)
        snprintf(out, cap, "%s", v ? "on" : "off");
    else
        snprintf(out, cap, "%d", v);
}

static void say_setting(int which)
{
    char text[64], value[32];

    value_text(which, value, sizeof value);
    snprintf(text, sizeof text, "%s %s", settings[which].name, value);
    screen_say_text(text);
}

static void save(void)
{
    char text[256];
    size_t n = 0;
    int i;

    for (i = 0; i < SETTINGS; i++)
        n += (size_t)snprintf(text + n, sizeof text - n, "%s %d\n",
                              settings[i].key, settings[i].value);
    write_file("B:/TALLFREE.CFG", text, n);
}

/* Saved settings, if any, over the defaults and whatever SAY.TXT set; the
   voice first, since it brings a pitch of its own. */
void settings_load(int volume, int abbreviations)
{
    const char *t = read_text("B:/TALLFREE.CFG");
    int i;

    settings[VOLUME].value = volume;
    settings[ABBREVIATIONS].value = abbreviations;
    settings[SPEED].value = speech_voice_get(6);
    settings[PITCH].value = speech_voice_get(2);
    while (t != NULL && *t) {
        for (i = 0; i < SETTINGS; i++) {
            size_t k = strlen(settings[i].key);
            if (strncmp(t, settings[i].key, k) == 0 && t[k] == ' ') {
                int v = atoi(t + k + 1);
                if (v >= settings[i].min && v <= settings[i].max)
                    settings[i].value = v;
            }
        }
        t = strchr(t, '\n');
        if (t != NULL)
            t++;
    }
    if (settings[VOICE].value != 1) {
        int pitch = settings[PITCH].value;
        apply(VOICE);
        settings[PITCH].value = pitch;
    }
    for (i = 0; i < SETTINGS; i++)
        if (i != VOICE)
            apply(i);
    printf("settings: speed %d, pitch %d, voice %d, volume %d, abbreviations %d, reading %d\n",
           settings[SPEED].value, settings[PITCH].value, settings[VOICE].value,
           settings[VOLUME].value, settings[ABBREVIATIONS].value, settings[READING].value);
}

int menu_is_open(void)
{
    return open_now;
}

/* One control, already kept from the interface; answers 1 when the menu
   has just closed, so the screen can be announced after it. */
int menu_action(int action, int step)
{
    int s;

    switch (action) {
    case MENU_OPEN:
        open_now = 1;
        editing = 0;
        screen_say_text("Speech settings");
        say_setting(current);
        return 0;
    case MENU_CLOSE:
        open_now = 0;
        editing = 0;
        save();
        screen_say_text("Settings saved");
        return 1;
    case MENU_PRESS:
        editing = !editing;
        if (editing) {
            char value[32];
            value_text(current, value, sizeof value);
            screen_say_text(value);
        } else {
            screen_say_text(settings[current].name);
        }
        return 0;
    case MENU_TURN:
        if (step == 0)
            return 0;
        if (!editing) {
            current = (current + (step > 0 ? 1 : SETTINGS - 1)) % SETTINGS;
            say_setting(current);
            return 0;
        }
        s = settings[current].value + (step > 0 ? 1 : -1) * settings[current].step;
        if (s < settings[current].min)
            s = settings[current].min;
        if (s > settings[current].max)
            s = settings[current].max;
        settings[current].value = s;
        apply(current);
        {
            char value[32];
            value_text(current, value, sizeof value);
            screen_say_text(value);
        }
        return 0;
    }
    return 0;
}
