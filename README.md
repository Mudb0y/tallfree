# Tallfree

A screen reader for the Roland SP-404MKII. The SP-404MKII has no accessibility
features at all, so a blind musician cannot reach any of its menus, parameters
or sample names. Tallfree is a modified firmware that speaks what is on the
screen, with the voice synthesised on the instrument itself by
[OpenEVV](https://github.com/Mudb0y/openevv), a reimplementation of the
Eloquence speech engine.

Tallfree is not made, endorsed or supported by Roland. Installing it is at your
own risk, although, as described below, the instrument can always be put back
to Roland's firmware.

## What it does

- Entering a screen says its title and the focused item. Moving the focus says
  the new item, and turning a value says the new value.
- A setting is read with its name and value; a tabbed page says the tab when it
  changes; a dialog is read on its own, then its highlighted button.
- The main screen says the bank and pad when you arrive and when the bank
  changes. Hitting pads is silent, except on screens that pick a pad, such as
  sampling, where each pad is named.
- The speech mixes into the main outputs and the headphones, over whatever the
  instrument is playing.

### Speech settings

Hold SHIFT and press EXIT to open the speech settings from any screen. Turn
VALUE to move between Speed, Pitch, Voice, Volume, Abbreviations and Screen
reading. Press VALUE to change the one you are on, turn to change it, and press
again to go back. Press EXIT to save and close; the settings are kept in the
instrument's internal memory.

## What it changes

Tallfree is Roland's SP-404MKII System Program Ver.5.52 with 13 bytes changed
and a small boot loader added. The speech engine needs memory the instrument
does not have spare, so **the looper and skip-back buffer is halved, from 27.3
to 13.65 seconds a channel.** Nothing else about the instrument changes.

## Installing

You need an SD card and a computer. From the latest release, download
`SP404MKII_APP1.bin`, the firmware, and `ENGINE.BIN`, the speech engine.

1. Format the SD card as FAT32 on the computer. Use a freshly formatted card:
   leftover deleted files can stop the instrument finding the update.
2. Copy `SP404MKII_APP1.bin` to the top level of the card. Make a folder called
   `EVV` on the card and copy `ENGINE.BIN` into it.
3. With the instrument off, put the card in. Hold SHIFT, switch on, and press
   the VALUE knob. The update takes about 90 seconds and makes no sound; when it
   is done the screen says `UPDATE COMPLETE!`. If you can, check that with a
   phone app that reads text aloud. Otherwise wait at least three minutes. Do
   not switch off during the update.
4. Switch off, then on again normally. You will hear "speech on".

Within a few seconds of starting, the engine copies itself into the
instrument's internal memory, so from then on it starts without the card. To
update the engine, put a new `ENGINE.BIN` in the card's `EVV` folder and
restart; it replaces the internal copy.

If you hear "No speech engine found", the firmware is installed but found no
engine on the card or in internal memory. Put `ENGINE.BIN` in the card's `EVV`
folder and restart.

### Settings file

A text file `EVV/SAY.TXT` on the card is read at start-up. Each line is either
a setting or something to say:

- `#vol N` sets the speech volume, 50 by default. The speech settings menu,
  once saved, takes precedence.
- `#mode off` starts with screen reading off; `#mode all` speaks every change
  on the screen instead of reading it as a screen reader would.
- `#dict on` turns the engine's abbreviation dictionary on.
- `#log on` writes logs to the card, which help with a bug report. Leave it off
  otherwise.
- Any other line is spoken when the instrument starts.

## If something goes wrong

- If the instrument stops starting after an engine update, put an empty file
  named `NOENGINE` in the card's `EVV` folder. The firmware then starts without
  loading any engine.
- To go back to Roland's firmware, download the system program from Roland's
  website and install it exactly as in step 3 above. The updater cannot touch
  the part of the instrument that runs it, so this always works.

## Building from source

OpenEVV is a submodule, pinned to the commit the engine was tested with, so
fetch it after cloning. The rest builds in Nix development shells:

    git submodule update --init
    cd engine
    (cd openevv && nix develop -c make RULES=bytecode build/evv)
    nix develop .. -c make lib device sims check

The third line builds the desktop OpenEVV in its own shell, for the
comparison. `make device` writes `engine/build/ENGINE.BIN`, `make sims` runs
the screen reader against recorded screens under QEMU, and `make check`
compares the engine's speech with the desktop OpenEVV's, sample for sample,
with no more memory than the instrument has. To build the
firmware, put the `SP404MKII_APP1.bin` from Roland's SP-404MKII System Program
Ver.5.52 in `firmware/` and run `python3 image/make_image.py`. `CLAUDE.md`
explains how all of it works.

## Licence

Tallfree's own code is under the MIT licence in `LICENSE`. The firmware in the
releases contains Roland's system program, which is Roland's. `ENGINE.BIN`
contains OpenEVV, under the MIT licence, and IBM's language data, on the terms
set out in [OpenEVV's NOTICE](https://github.com/Mudb0y/openevv/blob/main/NOTICE).
Roland and SP-404 are trademarks of Roland Corporation.
