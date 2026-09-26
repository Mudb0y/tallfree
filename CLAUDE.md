# Tallfree — custom firmware for the Roland SP-404MKII

Making the SP-404MKII speak. The instrument has no accessibility of any kind,
so to a blind musician every menu, parameter and sample name on it is
unreachable. Tallfree is a modified firmware that says what is on the screen,
with [OpenEVV](https://github.com/Mudb0y/openevv), a reimplementation of IBM's
Embedded ViaVoice, which sounds much like Eloquence, synthesising on the
device rather than playing pre-rendered clips.

The name comes from the first patched image, which renamed the USB product
string to `Roland SP-404TALL` to prove a three-byte change would run.

This file holds what is always true. The code's own comments are the detailed
record: `engine/screen.c` says what each hook is for and how the screen is
read, `image/boot.c` how the engine is loaded, `image/make_image.py` every
byte the image changes and why.

## Status, 26 September 2026

**It is a working screen reader.** Image 32 starts OpenEVV at boot, from the
card or the eMMC, and it reads the screen as a screen reader should: on
entering a screen its title and the focused item, then only what changes;
settings rows with their values, tabs, dialogs on their own, the main
screen's bank and pad but never a bare pad hit, pads in the modes that pick
them. SHIFT + EXIT opens a spoken speech settings menu, saved on the eMMC.
Speech mixes into the main and headphone outputs, and with compiled rules the
median time from a change on the screen to its sound is under 50 ms, with no
stalls at speed 135.

How the screen is read: hooks, all data writes, on the seven surface
vtables' DrawString, clear, fill and rectangle outline; the 76 icon menus'
icon draw; the SYSTEM page's row draw; the tab strip's draw, which names
the current tab; and the 94 page handlers, which also carry every key and
knob. Read the kernel's and the firmware's own code
before trusting any record layout or vtable slot.

Not solved: say-all and speech off, which the key hook makes easy; dialogs
closing, if they are not pages; which pad deletion mode points at.

## The hardware, in one paragraph

NXP i.MX RT1060, Cortex-M7, running T-Kernel. No Linux, no filesystem in the
application flash. 4 MB QSPI NOR at 0x60000000, 64 MB SDRAM at 0x80000000. The
application is copied to SDRAM at 0x80000000 and runs from there, so any byte of
the running program is at `file offset - 0x21AC0 + 0x80000000`. A companion
microcontroller — Roland calls it the BMC — hosts the USB audio and MIDI
interface, holds the i.MX boot mode lines, survives its reset, and is spoken to
in four-byte packets over LPUART3 at 0x4018C000.

## The build and test cycle

Everything happens through the SD card. Nothing needs the SP on USB.

Every command here runs inside `nix develop` at the top of the repo: the ARM
compiler, QEMU, make, and a python3 with capstone and numpy, from the nixpkgs
revision in `flake.lock`. `nix develop -c CMD` runs one. Each run after an
edit copies the tree into the Nix store, which is why the Ghidra scripts fetch
Ghidra by revision instead of through the flake.

Roland's firmware is not in the repo. Building the image or the Ghidra project
needs Roland's SP-404MKII system program 5.52, from Roland's download page,
unpacked into `firmware/`: `SP404MKII_APP1.bin`, sha256
`4a3d67711e14dcc97d50249a4eee7dd6df0251a2f37757cbbe2556c233730d80`.

**An engine change is a file copy and a restart.** Build `engine/`, run
`make sims` and `make check`, copy `engine/build/TALLFREE.BIN` to the card as
`A:/TALLFREE/TALLFREE.BIN`, power the unit off and on. The engine then copies
itself to the eMMC as `B:/TALLFREE.BIN` five seconds in, so it also starts
with no card.

**A flash** is for the boot loader or the image itself:

1. `image/make_image.py` builds the image into `image/build/` from
   `firmware/SP404MKII_APP1.bin`, compiling `image/boot.c` itself. It appends
   the payload at flash 0x308000 and checks every word it changes. A new image
   is an edit to this script and a commit, with the number in its first line
   moved on.
2. Format the SD card FAT32 and copy the image as `SP404MKII_APP1.bin`. Format
   rather than overwrite: a deleted directory entry ahead of the file makes the
   updater say NO UPDATER.
3. Hold SHIFT, power on, press the VALUE knob, wait about 90 seconds.
4. **Confirm the screen says UPDATE COMPLETE! before doing anything else.**
   This is not optional — see the hazards.

**A release** is one zip laid out as the card: `python3 release/make_zip.py
[yyyy.mm.dd]` rebuilds the image and the engine and packs Tallfree's
`SP404MKII_APP1.bin`, Roland's untouched `SP404MKII_APP0.bin` (the companion
chip's half of 5.52, checked against Roland's checksum) and
`TALLFREE/TALLFREE.BIN` into `release/build/tallfree-<date>.zip`, the same
bytes for the same tree. The version is the date of publishing; the release
title adds "(SP-404MKII System Program Ver.5.52)". The updater rewrites the
companion chip whenever APP0 is on the card, so users are told to delete
both firmware files once the update is done.

**If an engine stops the unit starting**, a card holding a file
`A:/TALLFREE/NOENGINE` makes the boot loader load nothing; flashing Roland's
original is the fallback behind that.

## Rules that were learned the hard way

**Confirm every flash before touching anything.** An unverified flash that had
not completed looked exactly like a bricked unit and cost an evening. While the
SP was on USB the product string confirmed this automatically; off USB nothing
does, so it needs a deliberate check.

**Keep the SP off a Linux host's USB.** PipeWire and ALSA probe any USB audio
device that appears; this one stops answering, and the host's whole audio
graph stalls, even with nothing linked or recording. Measure by recording its
analogue output instead.

**Measure, do not reason.** Four wrong answers in one evening came from
deriving numbers through stacked assumptions. Every one fell out immediately
once something was recorded. **The Cortex cycle counter is untrustworthy here**
— it was out by roughly ninefold — so time things by recording the instrument's
analogue output, not on the device.

**Peripheral windows are sparse.** Reading the eDMA control block linearly
bus-faulted the processor and took the power switch with it, because everything
between offset 0x48 and the descriptor array at 0x1000 is unimplemented. Read
named registers, never ranges. RAM is safe to sweep; registers are not.

**Recovery is real and has been used.** The SHIFT-plus-power updater erases only
flash 0x80000 to 0x3FF000 and programs from 0x80000, so the FlexSPI config
block, IVT, DCD and boot code below 0x80000 cannot be touched by anything we
flash. A bad application is always recoverable by flashing Roland's original
`SP404MKII_APP1.bin` the same way.

**The card can hang the unit.** A card whose FAT is inconsistent (a wrong free
count, orphaned clusters) hangs the unit's file system on the first write.
Image 30's boot loader wrote at start-up and locked the unit that way; image
31's writes nothing. Check the card with `fsck.fat -n` after any session that
logged to it.

## Audio: the working recipe

```
hook the eDMA channel-3 vector at 0x0000004C   (VTOR is 0, ITCM is writable)
read EDMA_INT bit 3 FIRST, then call the original handler (it clears the bit)
re-read the four descriptors at 0x400E9000 + n*32 EVERY interrupt
write 64 samples per interrupt; to mix, ADD each to line 3 words 14 and 15 only
keep |sample| below 2^19 — a 16-bit source shifted left by 3
restore the vector when finished
```

750.01 interrupts per second, one every 1.3333 ms, exactly 64 samples at 48 kHz.
A frame is 16 words. The buffers ping-pong between two sets — eight in all, at
0x20000000/0x20001000 through 0x20006000/0x20007000 — so a cached address writes
into the set that is not playing half the time. **The field wraps at 2^20**,
which is a symmetric nonlinearity and destroys a sine while leaving a square wave
untouched.

## Measuring by recording

Record the SP's analogue outputs into any audio interface at 48 kHz; the
python3 in `nix develop` has numpy for the analysis.

Design probes so the answer does not depend on unknowns. The measurement that
finally worked was a square wave at exactly half the interrupt rate: it gave the
rate independent of the core clock, the frame width and the samples per buffer,
all three of which were wrong at the time.

## Tools

Ghidra headless needs an **absolute** project path, and its script output only
carries the `INFO  <Script>.java>` prefix on the first line, so multi-line
decompiler output has to be extracted with `awk` from a marker onwards.

```
nix shell github:NixOS/nixpkgs/REV#ghidra --command ghidra-analyzeHeadless \
    "$PWD/ghidra/project" sp404 -process sdram_code.bin \
    -noanalysis -scriptPath "$PWD/ghidra/scripts" -postScript Foo.java
```

REV is the nixpkgs revision in `flake.lock`, which `ghidra/gdec` reads the
same way, so the project is only opened by the Ghidra the lock names.

`ghidra/gdec ADDR...` decompiles the functions containing those addresses. The
project is not in git; `ghidra/import.sh` builds it from `firmware/` in
about seven minutes, function-coverage repair included, to 8,366 functions.

Capstone needs `CS_ARCH_ARM` with `CS_MODE_THUMB | CS_MODE_MCLASS`; without the
M-class flag it stops at the first `msr basepri`. Do not name a script `dis.py`.

A vtable address found by scanning code for movw/movt is the start of the
table's header; constructors store it **plus 8**, past offset-to-top and RTTI,
so count slots from there. Reading from the loaded address gives the wrong
function two slots over. `ghidra/regions/itcm_code.bin` starts at runtime 0x400.

## How the engine runs

**The engine, as it runs now.** OpenEVV is one image,
`engine/build/TALLFREE.BIN`, linked to run at 0x83AC0000, header "EVV2",
with OpenEVV's rules compiled to C (RULES=c). Images 30 to 32 point the main
screen's status-line draw, vtable word 0x80226E98, at `image/boot.c` in
flash; on its first call that puts the word back and loads the engine from
the card, else the eMMC, checking its header and CRC, or plays a clip saying
there is no engine; it refuses image 29's "EVV1" engines, linked 1 MB
higher. Since image 31 the boot loader writes nothing to the card and passes
the engine where it found it instead; image 32's looks in `A:/TALLFREE/`,
where 31's looked in `A:/EVV/`. The room comes from the looper and skip-back
buffer, cut from 7.25 MB to 2.5 MB (13.65 s a channel). `engine/` builds it:
OpenEVV is the submodule `engine/openevv`, pinned to the commit the engine
was tested against (`git submodule update --init`), and `make lib` builds
its library there with PORT=none into its `build/m7c`; `make check` runs the
engine under QEMU's Cortex-M7 against the desktop engine built from the same
tree, and `cases.sh run` then `compare` does 80 cases. QEMU starts the
engine on junk memory, because the instrument does, and gives it no more
heap than the instrument is sure of, 1280 KB, which the device link asserts;
the harnesses stream their output rather than store it there.

**The engine is resident from boot.** It says "ready", speaks any script
in `A:/TALLFREE/TALLFREE.DEBUG` (its settings, `#vol`, `#mode`, `#settle`,
`#frame`, `#dict`, `#log`, `#out`, `#wait`, `#slots`, `#rxprobe`, are listed
at the top of `engine/service.c`), then hooks the drawing vtables and reads
the screen until switched off. It runs in a kernel task at priority 30;
inside it, OpenEVV's cooperative scheduler still runs synthesis. **`make
sims` runs the service and the hook under QEMU** against scenarios in
`engine/sim/*/`, written by `sim/scenarios.py` from what the unit's logs
recorded, each with the batches it must say; run it before every card test.
With `#log on` in `TALLFREE.DEBUG`, each run leaves `LOG.TXT` (where the
engine was loaded from, phrase timings, kernel-code checksum), `DRAWS.TXT`
(every change the hook saw), and `FAULT.TXT` if the fault catcher fired,
checkpointed before the hook goes in and every ten seconds after; without it
nothing goes to the card.

**Hook text by vtable swap.** Every string on the normal screens ends in
`FUN_800EE530(this, x, y, str, len)`, the surface's DrawString at vtable slot
0x12C, which has no direct callers: only seven vtable words reference it,
0x8021D2D0, 0x8021D46C, 0x8021D608, 0x8021D7A4, 0x8021E3D0, 0x8021E564 and
0x8021E700. Repointing those is a data write, so the I-cache problem does not
arise. Patching instructions in SDRAM would need the D-cache cleaned to the
point of unification and the I-cache invalidated; `dsb`/`isb` alone is not
enough on an M7. Menu icons carry baked-in words and bypass the text path.

**Slots, measured.** Only SAI line 3 reaches the main outputs: left from
words 0, 2, 4, 6, 12 and 14, right from 1, 3, 5, 7, 12 and 15, each at unity.
Writing all sixteen summed six copies per side, which is why early speech was
so loud. The firmware's mixer (ITCM 0x1C2D0) fills words 0 to 11, 14 and 15
with its fourteen bus channels, word 12 with a mono stream of its own, the
metronome, and word 13 with a bitmask of the buses in use, not audio.
Words 8 to 11 and 13 do not reach the main outputs.

**What the recorder hears.** Sampling and resampling both take audio only
from the receive side (eDMA channel 4, 0x20008000), and the hardware sends
back on receive words 0 and 1, digitally, the sum of line 3 words 0 to 7,
and never words 12, 14 or 15 (`#rxprobe` measures it). So speech is added to
**words 14 and 15**, clamped to +/-2^19, where recordings cannot catch it;
in words 0 and 1 it was sampled. Word 12 is no better: the click's Output
Assign silences it. `#out` chooses among them, `#vol N` sets the level, 50
by default. `TALLFREE.DEBUG` beginning `#slots` runs the 64-tone probe, and
`engine/slotmap.py` reads a recording of it, taking the two loudest channels
as the SP's outputs unless told which.
