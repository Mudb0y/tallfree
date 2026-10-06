/* What the screen draws, as a screen reader would speak it.

   Every string on the normal screens goes through one DrawString,
   FUN_800EE530(surface, x, y, string, length), and every clear through one
   of two calls, the whole surface (vtable slot 0x08) or a rectangle (slot
   0xC0), and every rectangle's outline through slot 0xBC. All four are
   reached only through seven surface vtables, so
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
#define SLOT_FRAME 0xBCu
#define SLOT_INK   0x10u
#define SLOT_MARK  0x18u
#define SLOT_TEXT  0x12Cu

#ifndef SIM
#define DRAW_STRING     0x800EE531u
#define DRAW_STRING_ASM "0x800EE531"
#define CLEAR_ALL       0x800F14D1u
#define CLEAR_ALL_ASM   "0x800F14D1"
#define FILL_RECT       0x800EDEE9u
#define FILL_RECT_ASM   "0x800EDEE9"
/* A rectangle's outline, slot 0xBC, the same in all seven classes, given
   four inclusive corners; some screens mark their choice with one. Most
   outlines are drawn through FUN_800CE548(ctx, rect), which adds the
   context's origin and keeps its caller's return seven words up. */
#define FRAME_RECT      0x800EDE11u
#define FRAME_RECT_ASM  "0x800EDE11"
#define WRAP_FRAME      0x800CE577u
static const uint32_t vt_base[] = {
    0x8021D1A4u, 0x8021D340u, 0x8021D4DCu, 0x8021D678u,
    0x8021E2A4u, 0x8021E438u, 0x8021E5D4u,
};
/* Icon menus draw each icon through FUN_80133E70(page, x, y, selected,
   selected picture, unselected picture), the page vtable slot 0x144 of all
   76 page classes that have one; each draws through the base picture drawer
   FUN_80135D00, on the display's global drawing context at 0x80591B48. */
#define ICON_DRAW       0x80133E71u
#define ICON_DRAW_ASM   "0x80133E71"
#define ICON_CTX        (*(volatile uint32_t *)0x80591B48u)
/* Each page is built by its factory, one of 94 in the table at 0x8023B4FC,
   when the interface goes to it. The SYSTEM page's rows are drawn by
   FUN_801489B8(page, ctx, rect, ., ., selected, .), its vtable word at
   0x80226C08; the selected row alone gets an underline. */
#define PAGE_TABLE      ((volatile uint32_t *)0x8023B4FCu)
#define ROW_WORD        (*(volatile uint32_t *)0x80226C08u)
#define ROW_DRAW        0x801489B9u
#define ROW_DRAW_ASM    "0x801489B9"
/* A page's tab strip is drawn by FUN_80121478(widget), reached only through
   the vtable word at 0x80221D10. The widget knows
   which tab is current: its index at +0x320, the count at +0x328, the names
   at +0x29C, the drawing context at +0x84. The strip scrolls, four tabs to a
   view, so the page count cannot name the tab by position. */
#define TAB_WORD        (*(volatile uint32_t *)0x80221D10u)
#define TAB_DRAW        0x80121479u
#define TAB_DRAW_ASM    "0x80121479"
/* What the screens that ask for pads have chosen, which only the pads'
   lights show. FUN_800D94D8 reads the store at 0x82E01144: the samples
   picked for export, a byte a pad over all ten banks from 0x134 (its
   parameter 0xB); the projects picked for export, a byte each from 0x1D4
   (0xC); the pattern picked for export, a word at 0x1E4 (0xD); the pads
   picked for deletion, a word a pad from 0x200 (0x14). The current bank,
   0 to 9, is at 0x82E2CD1C, as FUN_800DDA38(0x82E009D0, 0) reads it. The
   export page, 85, keeps itself at 0x80CFD4E4 and the pad operations page,
   67, at 0x80CFD490; each keeps its mode at 0x1A90. */
#define PICK_STORE      0x82E01144u
#define BANK_NOW        (*(volatile int32_t *)0x82E2CD1Cu)
/* Each pad's playback settings, a record of 0xAC bytes a pad over all ten
   banks from 0x82E2CD08, as FUN_800DC6B8 reads them: GATE at 0xE0 (its
   parameter 0x6A), LOOP at 0xE4 (0x6B), BPM SYNC at 0xF0 (0x6E), and at
   0x10C (0x75) REVERSE in bit 0 and the ping pong loop in bit 1. The
   current pad in its bank, 0 to 15, is at 0x82E2CD20. */
#define PAD_STORE       0x82E2CD08u
#define PAD_NOW         (*(volatile int32_t *)0x82E2CD20u)
#define EXPORT_PAGE     (*(volatile uint32_t *)0x80CFD4E4u)
#define PADOPS_PAGE     (*(volatile uint32_t *)0x80CFD490u)
#define PAT_STORE       0x82DFFC88u
#define SEQ_STATE       (*(volatile uint32_t *)0x80591F60u)
#define STEP_COUNT      (*(volatile int32_t *)0x802E7430u)
#define STEP_SLOTS      (*(volatile int32_t *)0x802E7434u)
#define STEP_LAST       (*(volatile int32_t *)0x802E743Cu)
#define STEP_BITS       0x80B683DCu
static const uint32_t icon_slots[] = {
    0x8021CF38u, 0x8021DA78u, 0x8021DC38u, 0x8021DDFCu, 0x8021DFBCu, 0x8021E8B4u,
    0x8021EA40u, 0x8021EBCCu, 0x8021ED58u, 0x8021EEE4u, 0x8021F1F8u, 0x8021F9ACu,
    0x8021FCC8u, 0x8021FE54u, 0x8021FFE0u, 0x8022016Cu, 0x802202FCu, 0x80220488u,
    0x80220614u, 0x802207A0u, 0x80220A04u, 0x80220B90u, 0x80221220u, 0x802213ACu,
    0x80221538u, 0x8022191Cu, 0x80221AA8u, 0x80221C34u, 0x802227ACu, 0x8022293Cu,
    0x80222AC8u, 0x80222C5Cu, 0x80223298u, 0x80223424u, 0x802235B0u, 0x8022373Cu,
    0x80223A30u, 0x80223C2Cu, 0x80223EE4u, 0x802249BCu, 0x80224B48u, 0x80224E98u,
    0x80225028u, 0x802251B4u, 0x80225344u, 0x802254F8u, 0x802259A4u, 0x80226184u,
    0x802263D8u, 0x80226568u, 0x802266F4u, 0x80226880u, 0x802275E4u, 0x80227770u,
    0x80227A8Cu, 0x80227C18u, 0x80227DA4u, 0x80227F30u, 0x802280BCu, 0x8022824Cu,
    0x80228F30u, 0x802290BCu, 0x80229248u, 0x802293D4u, 0x80229560u, 0x802296ECu,
    0x80229878u, 0x80229EF0u, 0x8022A07Cu, 0x8022AADCu, 0x8022AC6Cu, 0x8022AE2Cu,
    0x8022B1C0u, 0x8022C404u, 0x8022CFA0u, 0x8022D1ECu,
};
#else
/* The simulator's stand-ins, from sim.c. */
extern volatile uint32_t sim_vtables[7][0x130 / 4], sim_site;
void sim_draw_string(void);
void sim_clear(void);
void sim_fill(void);
void sim_frame(void);
void sim_batch(const char *phrases, int count);
void sim_stop(void);
extern volatile uint32_t sim_icon_slots[2], sim_icon_ctx;
extern volatile uint32_t sim_page_table[94], sim_row_word, sim_tab_word;
extern volatile uint32_t sim_store[0x600 / 4], sim_export_page, sim_padops_page;
extern volatile uint32_t sim_padstore[], sim_patstore[], sim_seq_state;
extern volatile int32_t sim_step_count, sim_step_slots, sim_step_last;
extern volatile uint16_t sim_step_bits[];
extern volatile int32_t sim_padnow;
extern volatile int32_t sim_bank;
extern volatile uint32_t sim_scroll_owner;
#define PICK_STORE      ((uint32_t)(uintptr_t)sim_store)
#define BANK_NOW        sim_bank
#define PAD_STORE       ((uint32_t)(uintptr_t)sim_padstore)
#define PAT_STORE       ((uint32_t)(uintptr_t)sim_patstore)
#define SEQ_STATE       sim_seq_state
#define STEP_COUNT      sim_step_count
#define STEP_SLOTS      sim_step_slots
#define STEP_LAST       sim_step_last
#define STEP_BITS       ((uint32_t)(uintptr_t)sim_step_bits)
#define PAD_NOW         sim_padnow
#define EXPORT_PAGE     sim_export_page
#define PADOPS_PAGE     sim_padops_page
void sim_draw_icon(void);
void sim_draw_row(void);
void sim_draw_tabs(void);
#define TAB_WORD        sim_tab_word
#define TAB_DRAW        ((uint32_t)(uintptr_t)sim_draw_tabs)
#define TAB_DRAW_ASM    "sim_draw_tabs"
#define PAGE_TABLE      sim_page_table
#define ROW_WORD        sim_row_word
#define ROW_DRAW        ((uint32_t)(uintptr_t)sim_draw_row)
#define ROW_DRAW_ASM    "sim_draw_row"
#define ICON_DRAW       ((uint32_t)(uintptr_t)sim_draw_icon)
#define ICON_DRAW_ASM   "sim_draw_icon"
#define ICON_CTX        sim_icon_ctx
static const uint32_t icon_slots[] = {
    (uint32_t)(uintptr_t)&sim_icon_slots[0], (uint32_t)(uintptr_t)&sim_icon_slots[1],
};
#define DRAW_STRING     ((uint32_t)(uintptr_t)sim_draw_string)
#define DRAW_STRING_ASM "sim_draw_string"
#define CLEAR_ALL       ((uint32_t)(uintptr_t)sim_clear)
#define CLEAR_ALL_ASM   "sim_clear"
#define FILL_RECT       ((uint32_t)(uintptr_t)sim_fill)
#define FILL_RECT_ASM   "sim_fill"
#define FRAME_RECT      ((uint32_t)(uintptr_t)sim_frame)
#define FRAME_RECT_ASM  "sim_frame"
static const uint32_t vt_base[] = {
    (uint32_t)(uintptr_t)sim_vtables[0], (uint32_t)(uintptr_t)sim_vtables[1],
    (uint32_t)(uintptr_t)sim_vtables[2], (uint32_t)(uintptr_t)sim_vtables[3],
    (uint32_t)(uintptr_t)sim_vtables[4], (uint32_t)(uintptr_t)sim_vtables[5],
    (uint32_t)(uintptr_t)sim_vtables[6],
};
#endif
/* The external input, on or off, in the pick store at 0x5C (its parameter
   0x25), as FUN_80134A98 sets it: each press flips it, or with EXT SOURCE
   held and GATE pressed it follows the button, on while held. */
#define EXT_ON          (*(volatile uint32_t *)(PICK_STORE + 0x5Cu))
/* COPY BANK PAD's cursor, on the source bank (0) or the destination, at
   0x488 of the same store (its parameter 0x17), as VALUE moves it. */
#define COPY_SIDE       (*(volatile int32_t *)(PICK_STORE + 0x488u))
/* The pattern sequencer's store at 0x82DFFC88, as FUN_800E26D0 reads it.
   Its mode at 0x34 (parameter 0x77): 1 choosing patterns, and the pattern
   screen's operations, 6 DELETE, 7 DELETE BANK, 8 COPY, 9 COPY BANK, 10
   EXCHANGE. The pattern chosen in its bank, or -1, at 0x2B8 (0x79): COPY's
   and EXCHANGE's source. The bank the pads show patterns from at 0x2BC
   (0x7A), which the bank keys set in pattern mode, as FUN_80084258 and
   FUN_800C9788 do, and the sample bank at 0x82E2CD1C stays as it was. Once a
   source is chosen they set the destination's bank instead, at 0x2DC (0x82),
   or, choosing the samples a copy keeps, those samples' bank, at 0x2F0
   (0x94); on COPY BANK the bank the cursor is on, the source at 0x2E8 (0x8B)
   or the destination at 0x2E4 (0x8A), the cursor at 0x2EC (0x8C) and 0 on
   the source. FUN_801350B8 takes the pads: DELETE flips a word a pattern
   from 0x2F8 (0x85), and choosing the samples to keep a word a sample from
   0x57C (0x8E). */
#define PAT_WORD(off)   (*(volatile int32_t *)(PAT_STORE + (off)))
#define PAT_MODE        PAT_WORD(0x34u)
#define PAT_CHOSEN      PAT_WORD(0x2B8u)
#define PAT_BANK        PAT_WORD(0x2BCu)
#define PAT_DEST_BANK   PAT_WORD(0x2DCu)
#define PAT_KEEP_BANK   PAT_WORD(0x2F0u)
#define PAT_COPY_SIDE   PAT_WORD(0x2ECu)
#define PAT_DELETE(i)   PAT_WORD(0x2F8u + 4u * (uint32_t)(i))
#define PAT_KEEP(i)     PAT_WORD(0x57Cu + 4u * (uint32_t)(i))
enum { PMODE_DELETE = 6, PMODE_DELETE_BANK, PMODE_COPY, PMODE_COPY_BANK, PMODE_EXCHANGE };
/* TR-REC's steps as the pads' lights show them: the lights ask
   FUN_80062070, parameter 0x1D, which asks FUN_80034110. For the bar TR-REC
   shows, the sequencer keeps a word a note in each of its fine slots, 256
   bytes a slot from 0x80B683DC: bit 0 for a note up to 126, bit 1 for one
   above, kept 0x50 lower. A step is its share of the slots, their count at
   0x802E7434 over the steps' at 0x802E7430, below the last slot in use at
   0x802E743C; step N is pad N. The sequencer's state is the struct
   0x80591F60 points at: its mode at +0, 6 in TR-REC, and the note of the
   sample being input at +0x5A, 47 on from A 1's. */
#define SEQ_WORD(seq, off) (*(volatile int16_t *)((seq) + (off)))
#define SEQ_TRREC 6

static int seq_state(uint32_t *seq)
{
    uint32_t p = SEQ_STATE;

#ifndef SIM
    if (p < 0x80000000u || p >= 0x83A00000u || (p & 1u))
        return 0;
#endif
    *seq = p;
    return p != 0;
}

/* Whether step STEP of the bar shown holds NOTE, or -1 if that cannot be
   told. */
static int step_on(int step, int note)
{
    int32_t count = STEP_COUNT, slots = STEP_SLOTS, last = STEP_LAST, k, s;
    int high = note > 0x7E;

    if (high)
        note -= 0x50;
    if (count <= 0 || count > 64 || slots <= 0 || slots > 1024 || note < 0 || note > 127
        || step < 0 || step > 15)
        return -1;
    k = slots / count;
    for (s = step * k; s < (step + 1) * k && s < last; s++)
        if (*(volatile uint16_t *)(STEP_BITS + (uint32_t)s * 0x100u + (uint32_t)note * 2u)
            & (1u << high))
            return 1;
    return 0;
}

/* The bar's sixteen steps for NOTE, for the log. */
static unsigned step_mask(int note)
{
    unsigned m = 0;
    int k;

    for (k = 0; k < 16; k++)
        if (step_on(k, note) > 0)
            m |= 1u << k;
    return m;
}
#define VTABLES   (sizeof vt_base / sizeof vt_base[0])
#define ICON_SLOTS (sizeof icon_slots / sizeof icon_slots[0])
#define PAGES 94
#define ICON_SLOT(i) (*(volatile uint32_t *)icon_slots[i])
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
enum { ROLE_NONE, ROLE_TITLE, ROLE_TOAST, ROLE_PAD, ROLE_MAIN, ROLE_IGNORE, ROLE_TABS,
       ROLE_CHOICE, ROLE_EFFECT, ROLE_PICKS, ROLE_UNLIT, ROLE_FXLABEL, ROLE_HINT, ROLE_KNOB,
       ROLE_CELL, ROLE_QUIET, ROLE_STATE, ROLE_COPY_FROM, ROLE_COPY_TO, ROLE_NOTICE, ROLE_ANSWER,
       ROLE_MESSAGE };
#define CHAIN_TITLE     0x80141D3Fu
#define CHAIN_SLOT      0x8008D11Bu
#define CHAIN_SLOT_LIT  0x8008D19Bu
#define MICRO_TIMING    0x8015F007u
#define MICRO_PITCH     0x8015F09Bu
#define MICRO_VELOCITY  0x8015F0BDu
#define TRREC_PATTERN   0x8016C6C5u            /* TR-REC's Ptn:G1 */
#define BIG_TEXT        0x800EE779u            /* the big font's drawer, for
                                                  every screen */
static const struct { uint32_t site; uint8_t role; } sites[] = {
    { TITLE_SITE,  ROLE_TITLE },            /* the page title setter */
    { 0x80151801u, ROLE_TITLE },            /* EXPORT SAMPLE/PROJ./MULTIPAD */
    { 0x801120A1u, ROLE_TITLE },            /* CHROMATIC MODE */
    { 0x80146135u, ROLE_TITLE },            /* an effect's name, over its grid or page */
    { 0x80146175u, ROLE_TITLE },            /* the grid's MFX LIST 1-16 in its place, when
                                               the page shows none selected */
    { 0x80146545u, ROLE_EFFECT },           /* an effect's values, FUN_80146260, the
                                               effect page's alone */
    { 0x801463F7u, ROLE_FXLABEL },          /* and their names, CUTOFF, FEEDBACK */
    /* The CTRL knobs' columns on the pad and pattern settings, FUN_801709xx:
       the names, the values in the middle and the lines at the bottom. */
    { 0x801709ABu, ROLE_KNOB },
    { 0x80170A15u, ROLE_KNOB },
    { 0x801709EFu, ROLE_KNOB },
    { 0x80145EDFu, ROLE_CELL },             /* the effects grid's cells */
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
    { 0x80105033u, ROLE_TITLE },            /* the BPM screen's heading, C1:TEMPO SEL */
    { 0x8015604Bu, ROLE_PICKS },            /* SELECT PAD, TOT SELECTED PADS:1 */
    /* The pad operations page's other headings, centred in its status bar
       by FUN_80155768: COPY PAD, EXCHANGE PAD, COPY BANK PAD, DELETE BANK. */
    { 0x8015592Du, ROLE_TITLE },
    { 0x80155BCDu, ROLE_TITLE },
    { 0x80155C61u, ROLE_TITLE },
    { 0x80155D97u, ROLE_TITLE },
    { 0x80155DE7u, ROLE_TITLE },
    { 0x80155ED5u, ROLE_TITLE },
    { 0x80155F97u, ROLE_TITLE },
    { 0x80156139u, ROLE_TITLE },
    /* Text a copy or an exchange draws only in answer to the pads and keys:
       the pads it is between, A13 >> B1, on the pad operations page and the
       pattern screen's COPY and EXCHANGE, FUN_80148B50, and under COPY's,
       REMAIN: Samples to copy or Select Samples. */
    { 0x801561D5u, ROLE_ANSWER },
    { 0x80148DF3u, ROLE_ANSWER },
    { 0x80149013u, ROLE_ANSWER },
    { 0x8014910Du, ROLE_ANSWER },
    { 0x80149303u, ROLE_ANSWER },
    { 0x801495A1u, ROLE_ANSWER },
    { 0x80149869u, ROLE_ANSWER },
    /* TR-REC's sample for input, G-13 : TRIG, which SUB PAD and a pad
       choose, FUN_8016C4xx. */
    { 0x8016C75Fu, ROLE_ANSWER },
    /* The pattern chain's slots, FUN_8008D0xx, which the pads fill: the
       rest, and the highlighted one. Its heading, PATTERN CHAIN [1], with
       (*) once it is changed. */
    { CHAIN_SLOT, ROLE_ANSWER },
    { CHAIN_SLOT_LIT, ROLE_ANSWER },
    { CHAIN_TITLE, ROLE_TITLE },
    { 0x8016EBDBu, ROLE_IGNORE },           /* its position as it plays, 1.2, - stopped */
    /* The message box FUN_8001F4xx shows an operation's messages in, Working
       and Operation Completed!, and dialogs' questions, a line at a time:
       Working, Load Project 02. */
    { 0x8001F4BDu, ROLE_MESSAGE },
    { 0x8001F677u, ROLE_MESSAGE },
    { 0x8001F6B5u, ROLE_MESSAGE },
    /* The Microscope's values, FUN_8015F0xx: the timing VALUE moves, the
       pitch, CHROM:0, and the velocity; the timing is on white. */
    { MICRO_TIMING, ROLE_KNOB },
    { MICRO_PITCH, ROLE_KNOB },
    { MICRO_VELOCITY, ROLE_KNOB },
    /* TR-REC's PITCH knob's mode under its name, CHROMATIC or PAD. */
    { 0x801709C7u, ROLE_KNOB },
    /* COPY BANK PAD's warning, (PAD will be overwritten), and the banks it
       copies from and to, either side of >>. */
    { 0x80155C7Fu, ROLE_NOTICE },
    { 0x8015608Du, ROLE_COPY_FROM },
    { 0x8015609Bu, ROLE_IGNORE },
    { 0x801560A9u, ROLE_COPY_TO },
    /* The same on the pattern screen's COPY BANK, as FUN_80148B50 draws it:
       (PATTERN will be overwritten), and the banks either side of >>. */
    { 0x801496DDu, ROLE_NOTICE },
    { 0x8014989Fu, ROLE_COPY_FROM },
    { 0x801498ABu, ROLE_IGNORE },
    { 0x801498B7u, ROLE_COPY_TO },
    { 0x80151927u, ROLE_TITLE },            /* the export page's heading over PLEASE
                                               SELECT SMPL, FUN_801518B0 */
    { 0x8015F85Fu, ROLE_STATE },            /* the pattern screen's SELECT, STOP-PTN C1,
                                               PLAY-PTN C1 */
    { 0x8016B061u, ROLE_UNLIT },            /* the pattern settings' quantise grid,
                                               GRID 16, on white */
    { 0x80145629u, ROLE_UNLIT },            /* the pad settings' tempo mode, MANU */
    { 0x800F769Fu, ROLE_HINT },             /* and VALUE under BPM SET, the knob for it */
    /* The markers along a waveform, which he places by ear: sample edit's
       start, end, loop and cursor, and auto mark's marks, the selected one
       on white. */
    { 0x8014BBB3u, ROLE_IGNORE },           /* S */
    { 0x8014BCA5u, ROLE_IGNORE },           /* E */
    { 0x8014BC33u, ROLE_IGNORE },           /* L */
    { 0x8014B8A9u, ROLE_IGNORE },           /* C */
    { 0x8014B237u, ROLE_IGNORE },           /* M, selected */
    { 0x8014B4E1u, ROLE_IGNORE },           /* M */
    /* An outline that marks the chosen one of several strings. */
    { 0x80105153u, ROLE_CHOICE },           /* the BPM screen's, round PROJECT or the bank */
};

static uint8_t role_of(uint32_t site)
{
    unsigned i;

    for (i = 0; i < sizeof sites / sizeof sites[0]; i++)
        if (sites[i].site == site)
            return sites[i].role;
    return ROLE_NONE;
}

static volatile int unload_flag, screen_hooked, icons_hooked, rows_hooked, tabs_hooked,
    pages_hooked,
    screen_mode;

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
enum { EV_TEXT, EV_CLEAR, EV_FILL, EV_ICON, EV_ROW, EV_PAGE, EV_MENU, EV_FRAME, EV_TAB };
#define DRAW_TEXT 64
struct draw {
    uint32_t tick, surf, lr, site;
    int32_t  mark, ink;
    int16_t  x, y, x1, y1;
    uint8_t  kind, task, len, sample;
    char     text[DRAW_TEXT];
};

#define DRAWS 1024
static struct draw draws[DRAWS];
static volatile uint32_t draw_wr, draw_rd, draw_lost, draw_seen;

void text_hook(void);
void clear_hook(void);
void fill_hook(void);
void frame_hook(void);
void icon_hook(void);
void row_hook(void);
void tab_hook(void);
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
HOOK(frame_hook, frame_record, FRAME_RECT_ASM);
HOOK(icon_hook, icon_record, ICON_DRAW_ASM);
HOOK(row_hook, row_record, ROW_DRAW_ASM);
HOOK(tab_hook, tab_record, TAB_DRAW_ASM);

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

/* The kinds of file the firmware takes as samples, by a name's ending. */
static int sample_name(const char *t)
{
    static const char *const ext[] = { ".wav", ".aif", ".aiff", ".mp3" };
    size_t n = strlen(t), k, j;
    unsigned i;

    for (i = 0; i < sizeof ext / sizeof ext[0]; i++) {
        k = strlen(ext[i]);
        if (n <= k)
            continue;
        for (j = 0; j < k && (t[n - k + j] | 0x20) == ext[i][j]; j++)
            ;
        if (j == k)
            return 1;
    }
    return 0;
}

/* Two lists scroll a name too long for its column by drawing it again with
   its first characters dropped, until what is left fits, and then whole
   again. The file lists import and export choose from, FUN_80154DC0, draw
   the focused row from a copy of what their slot 0x10C, FUN_801554E0, gives
   it, a character further every ten draws; the list keeps the whole name at
   +0x35C and is in r5 as it calls FUN_800D80E0. REMAIN's NAME, FUN_8008C4A0,
   draws its own copy a character further every draw, the count at +0x1A8C
   of the page, which is in r5 likewise. FUN_800D80E0 pushes r4 to r7 under
   its return, so the caller's r5 is two words above the stacked length.
   The file lists also draw the folder "..", which goes back up, as ^. */
#define SCROLL_LIST     0x80155187u
#define SCROLL_REMAIN   0x8008C701u
#ifndef SIM
#define SCROLL_OWNER(f) ((f)[8 + 2])
#else
#define SCROLL_OWNER(f) sim_scroll_owner
#endif

/* The whole of a scrolling name, given the part of it drawn, STR, *LEN
   long, or NULL; *LEN becomes its length. The reader takes the name for
   the same text however far it has scrolled. */
static const char *scroll_whole(const uint32_t *f, uint32_t site, const char *str, int *len)
{
    uint32_t owner;
    const char *whole;
    int32_t k;
    int n;

    if (site != SCROLL_LIST && site != SCROLL_REMAIN)
        return NULL;
    owner = SCROLL_OWNER(f);
#ifndef SIM
    if (owner < 0x80000000u || owner >= 0x83A00000u || (owner & 3u))
        return NULL;
#else
    if (owner == 0)
        return NULL;
#endif
    if (site == SCROLL_LIST) {
        whole = (const char *)(owner + 0x35Cu);
    } else {
        k = *(volatile int32_t *)(owner + 0x1A8Cu);
        if (k < 0 || k > 63)
            return NULL;
        whole = str - k;
    }
    if (site == SCROLL_LIST && *len == 1 && str[0] == '^' && strcmp(whole, "..") == 0) {
        *len = 2;
        return whole;
    }
    for (n = 0; n < 256 && whole[n]; n++)
        ;
    if (n == 256 || n < *len || strncmp(whole + n - *len, str, (size_t)*len) != 0)
        return NULL;
    *len = n;
    return whole;
}

/* f holds r0 to r5, r12 and lr as the caller left them, then the caller's
   stack from the stacked length up. Slot 0x18 is the surface's background
   colour, the getter DrawString itself asks. */
__attribute__((used)) static void text_record(const uint32_t *f)
{
    struct draw d;
    const char *str = (const char *)f[3], *whole = NULL;
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
    if (len > 0 && (whole = scroll_whole(f, d.site, str, &len)) != NULL)
        str = whole;
    d.mark = ((int32_t (*)(uint32_t))(*(const uint32_t *const *)surf)[SLOT_MARK / 4])(surf);
    /* Slot 0x10, the text colour, a plain getter in all seven classes. */
    d.ink = ((int32_t (*)(uint32_t))(*(const uint32_t *const *)surf)[SLOT_INK / 4])(surf);
    d.x = (int16_t)f[1];
    d.y = (int16_t)f[2];
    d.x1 = d.y1 = 0;
    d.task = (uint8_t)kernel_task_self();
    d.len = (uint8_t)(len < 0 ? 0 : len > 255 ? 255 : len);
    for (n = 0; n < len && n < DRAW_TEXT - 1 && str[n]; n++)
        d.text[n] = str[n];
    d.text[n] = 0;
    /* From the whole name, as a name longer than the text kept loses its
       ending. */
    d.sample = d.site == SCROLL_LIST && sample_name(whole != NULL ? whole : d.text);
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

/* An outline, its corners as the fill's, and the code that drew it when that
   went through FUN_800CE548. */
__attribute__((used)) static void frame_record(const uint32_t *f)
{
    struct draw d;
    const int32_t *r = (const int32_t *)f[1];

    if (!screen_hooked || f[0] == 0 || r == NULL)
        return;
    memset(&d, 0, sizeof d);
    d.kind = EV_FRAME;
    d.tick = device_ticks();
    d.surf = f[0];
    d.lr = f[7];
#ifndef SIM
    d.site = d.lr == WRAP_FRAME ? f[8 + 7] : 0;
#else
    d.site = sim_site;
#endif
    d.x = (int16_t)r[0];
    d.y = (int16_t)r[1];
    d.x1 = (int16_t)r[2];
    d.y1 = (int16_t)r[3];
    push(&d);
}

/* An icon: where it lands, from the global context's surface and origin,
   whether it is the selected one, and the tail of its picture's name. */
__attribute__((used)) static void icon_record(const uint32_t *f)
{
    struct draw d;
    const char *name = (const char *)f[8], *tail;
    uint32_t ctx = ICON_CTX;
    int n;

    if (!screen_hooked || ctx == 0)
        return;
    memset(&d, 0, sizeof d);
    d.kind = EV_ICON;
    d.tick = device_ticks();
    d.surf = *(const volatile uint32_t *)ctx;
    d.x = (int16_t)((int16_t)f[1] + *(const volatile int16_t *)(ctx + 0x14));
    d.y = (int16_t)((int16_t)f[2] + *(const volatile int16_t *)(ctx + 0x18));
    d.mark = f[3] != 0;
    d.site = f[7];
    tail = name;
    if (name != NULL)
        for (n = 0; name[n] && n < 200; n++)
            if (name[n] == '/')
                tail = name + n + 1;
    for (n = 0; tail != NULL && tail[n] && n < DRAW_TEXT - 1; n++)
        d.text[n] = tail[n];
    d.text[n] = 0;
    push(&d);
}

/* A settings row: where its text goes, from the drawing context's surface
   and origin and the row's rectangle, and whether it is the selected one,
   the sixth argument, on the stack. */
__attribute__((used)) static void row_record(const uint32_t *f)
{
    struct draw d;
    uint32_t ctx = f[1];
    const int16_t *rect = (const int16_t *)f[2];

    if (!screen_hooked || ctx == 0 || rect == NULL)
        return;
    memset(&d, 0, sizeof d);
    d.kind = EV_ROW;
    d.tick = device_ticks();
    d.surf = *(const volatile uint32_t *)ctx;
    d.y = (int16_t)(rect[2] + 1 + *(const volatile int16_t *)(ctx + 0x18));
    d.mark = f[9] != 0;
    push(&d);
}

/* A tab strip: the current tab's name, from the widget itself, and the
   surface it draws on, the first word of its drawing context. */
__attribute__((used)) static void tab_record(const uint32_t *f)
{
    struct draw d;
    uint32_t w = f[0], ctx;
    int32_t cur, n;
    const char *name;
    int k;

    if (!screen_hooked || w == 0)
        return;
    cur = *(const volatile int32_t *)(w + 0x320);
    n = *(const volatile int32_t *)(w + 0x328);
    ctx = *(const volatile uint32_t *)(w + 0x84);
    if (cur < 0 || cur >= n || n > 32 || ctx == 0)
        return;
    name = *(const char *const volatile *)(w + 0x29C + 4u * (uint32_t)cur);
    if (name == NULL)
        return;
    memset(&d, 0, sizeof d);
    d.kind = EV_TAB;
    d.tick = device_ticks();
    d.surf = *(const volatile uint32_t *)ctx;
    d.mark = cur;
    for (k = 0; name[k] && k < DRAW_TEXT - 1; k++)
        d.text[k] = name[k];
    d.text[k] = 0;
    push(&d);
}

/* The page handlers, their stubs, and what they were. Each is sent a
   message whose first halfword is its type; FUN_80063B68, the UTILITY
   page's, builds the page on type 1. The interface sends the page it is on
   other types twenty times a second, so only type 1 is a page change; the
   types each page is sent are logged as they change, to learn the rest. */
uint32_t page_orig[PAGES];
static int16_t page_last_type[PAGES];

/* The keys the speech settings menu uses: SHIFT, EXIT, pressing VALUE, and
   VALUE, knob 0. Measured from the unit's key log. */
#define KEY_SHIFT 0x2A
#define KEY_EXIT  0x22
#define KEY_VALUE_PRESS 0x31
#define KNOB_VALUE 0

static volatile int shift_down, keys_to_menu, exit_kept, press_kept;

static void menu_event(int action, int step)
{
    struct draw d;

    memset(&d, 0, sizeof d);
    d.kind = EV_MENU;
    d.tick = device_ticks();
    d.x = (int16_t)action;
    d.y = (int16_t)step;
    push(&d);
}

/* In the interface's own task, before the page sees a message: answers 1 to
   keep it from the page. SHIFT + EXIT opens the menu, and that EXIT is kept,
   so it does not stop everything as SHIFT + EXIT otherwise does; while the
   menu is open EXIT, pressing VALUE and turning VALUE are kept for it, and
   everything else, SHIFT and the pads included, goes through. A key's
   release is kept whenever its press was. */
static int keep_for_menu(int type, int code, int step)
{
    if (type == 5) {
        if (code == KEY_SHIFT) {
            shift_down = 1;
            return 0;
        }
        if (code == KEY_EXIT && (keys_to_menu || shift_down)) {
            exit_kept = 1;
            keys_to_menu = !keys_to_menu;
            menu_event(keys_to_menu ? MENU_OPEN : MENU_CLOSE, 0);
            return 1;
        }
        if (code == KEY_VALUE_PRESS && keys_to_menu) {
            press_kept = 1;
            menu_event(MENU_PRESS, 0);
            return 1;
        }
        return 0;
    }
    if (type == 6) {
        if (code == KEY_SHIFT)
            shift_down = 0;
        if (code == KEY_EXIT && exit_kept) {
            exit_kept = 0;
            return 1;
        }
        if (code == KEY_VALUE_PRESS && press_kept) {
            press_kept = 0;
            return 1;
        }
        return 0;
    }
    if (type == 7 && keys_to_menu && code == KNOB_VALUE) {
        menu_event(MENU_TURN, step);
        return 1;
    }
    return 0;
}

/* A pad pressed where a screen asks for pads: which choice it makes, by
   the page and its mode, as FUN_80151688 (export), FUN_8012E0B8 (pad
   operations) and FUN_801350B8 (the pattern screen) make it. The pad
   operations' other modes, copy and exchange, and the pattern screen's,
   draw their pads as text. */
#define PAGE_PADOPS 67
#define PAGE_EXPORT 85
#define PAGE_PATTERN 60
#define PAGE_CHAIN 57
#define PAGE_RECORD_SETTING 62
#define PAGE_MICROSCOPE 80
#define PAGE_REMAIN 49
#define PAGE_PROJECT 59
#define PAGE_MODE(p) (*(volatile int32_t *)((p) + 0x1A90u))
enum { PICK_NONE, PICK_SAMPLE, PICK_PROJECT, PICK_PATTERN, PICK_PADS, PICK_PATTERNS, PICK_KEEP,
       PICK_STEP };

static int pick_kind(uint32_t page, int pad, int *index)
{
    uint32_t p;
    int32_t bank = BANK_NOW;

    if (pad < 0 || pad > 15)
        return PICK_NONE;
    /* The pattern screen's DELETE, from the bank the pads show, and COPY
       with a source chosen, choosing the samples to keep from theirs; a pad
       that sets COPY's destination instead is said as the screen shows
       it. */
    if (page == PAGE_PATTERN) {
        int32_t mode = PAT_MODE;
        uint32_t seq;
        /* TR-REC: the pad's step, with the note of the sample it is for. */
        if (seq_state(&seq) && SEQ_WORD(seq, 0) == SEQ_TRREC
            && (mode < PMODE_DELETE || mode > PMODE_EXCHANGE)) {
            int note = SEQ_WORD(seq, 0x5A);
            if (note < 0 || note > 0xFF)
                return PICK_NONE;
            *index = pad | note << 8;
            return PICK_STEP;
        }
        bank = mode == PMODE_DELETE ? PAT_BANK : PAT_KEEP_BANK;
        if (bank < 0 || bank > 9)
            return PICK_NONE;
        *index = (int)bank * 16 + pad;
        if (mode == PMODE_DELETE)
            return PICK_PATTERNS;
        if (mode == PMODE_COPY && PAT_CHOSEN >= 0)
            return PICK_KEEP;
        return PICK_NONE;
    }
    if (bank < 0 || bank > 9)
        return PICK_NONE;
    if (page == PAGE_EXPORT && (p = EXPORT_PAGE) != 0) {
        switch (PAGE_MODE(p)) {
        case 0:
            *index = (int)bank * 16 + pad;
            return PICK_SAMPLE;
        case 1:
            *index = pad;
            return PICK_PROJECT;
        case 2:
            *index = 0;
            return PICK_PATTERN;
        }
    } else if (page == PAGE_PADOPS && (p = PADOPS_PAGE) != 0 && PAGE_MODE(p) == 1) {
        *index = (int)bank * 16 + pad;
        return PICK_PADS;
    }
    return PICK_NONE;
}

/* Whether a pad or project is chosen, or which pattern is. */
static int32_t pick_state(int kind, int index)
{
    switch (kind) {
    case PICK_SAMPLE:
        return *(volatile uint8_t *)(PICK_STORE + 0x134u + (uint32_t)index);
    case PICK_PROJECT:
        return *(volatile uint8_t *)(PICK_STORE + 0x1D4u + (uint32_t)index);
    case PICK_PATTERN:
        return *(volatile int32_t *)(PICK_STORE + 0x1E4u);
    case PICK_PADS:
        return *(volatile int32_t *)(PICK_STORE + 0x200u + 4u * (uint32_t)index);
    case PICK_PATTERNS:
        return PAT_DELETE(index);
    case PICK_KEEP:
        return PAT_KEEP(index);
    case PICK_STEP:
        return step_on(index & 0xFF, index >> 8);
    }
    return 0;
}

/* The playback buttons, BPM SYNC to REVERSE, keys 0x1D to 0x20, as
   FUN_800C9788 handles them, and a pad's settings for them, one bit each. */
#define KEY_BPM_SYNC 0x1D
#define KEY_REVERSE  0x20
#define KEY_EXT_SOURCE 0x12
enum { SET_GATE = 1, SET_LOOP = 2, SET_SYNC = 4, SET_REVERSE = 8, SET_PINGPONG = 16 };

static int32_t pad_settings(int index)
{
    uint32_t r = PAD_STORE + (uint32_t)index * 0xACu;
    uint32_t flags = *(volatile uint32_t *)(r + 0x10Cu);

    return (*(volatile uint32_t *)(r + 0xE0u) ? SET_GATE : 0)
        | (*(volatile uint32_t *)(r + 0xE4u) ? SET_LOOP : 0)
        | (*(volatile uint32_t *)(r + 0xF0u) ? SET_SYNC : 0)
        | (flags & 1u ? SET_REVERSE : 0) | (flags & 2u ? SET_PINGPONG : 0);
}

/* A screen copying a bank: the pad operations page in its mode 3, or the
   pattern screen's COPY BANK. */
static int copy_bank(uint32_t page)
{
    uint32_t p = PADOPS_PAGE;

    if (page == PAGE_PATTERN)
        return PAT_MODE == PMODE_COPY_BANK;
    return page == PAGE_PADOPS && p != 0 && PAGE_MODE(p) == 3;
}

/* Where its cursor is, 0 on the source bank. */
static int32_t copy_side(uint32_t page)
{
    return page == PAGE_PATTERN ? PAT_COPY_SIDE : COPY_SIDE;
}

/* The pattern store's banks, each a byte, as they were before a bank key:
   the bank the pads show, the destination's, and the samples' to keep. */
#define PAT_BANKS 3
static int32_t pat_banks(void)
{
    return (int32_t)(((uint32_t)PAT_BANK & 0xFFu) | ((uint32_t)PAT_DEST_BANK & 0xFFu) << 8
                     | ((uint32_t)PAT_KEEP_BANK & 0xFFu) << 16);
}

__attribute__((used)) static int page_record(uint32_t page, const int16_t *message)
{
    struct draw d;
    int16_t type;

    if (!screen_hooked || message == NULL || page >= PAGES)
        return 0;
    type = *message;
    if (type >= 5 && type <= 7 && keep_for_menu(type, message[1], message[2]))
        return 1;
    /* A pad pressed (type 16, the pad, 0 to 15, in the second halfword)
       where a screen asks for pads goes with whether it was chosen before
       the page sees it. */
    if (type == 16) {
        int index = 0, kind = pick_kind(page, message[1], &index);

        if (kind != PICK_NONE) {
            memset(&d, 0, sizeof d);
            d.kind = EV_PAGE;
            d.tick = device_ticks();
            d.mark = (int32_t)page;
            d.x = type;
            d.y = message[1];
            d.x1 = (int16_t)index;
            d.site = (uint32_t)index;
            d.y1 = (int16_t)kind;
            d.ink = pick_state(kind, index);
            push(&d);
            return 0;
        }
    }
    /* Keys, key releases and knob turns (types 5, 6 and 7, the key or knob
       in the second halfword, a knob's step in the third) and the CTRL
       knobs (type 9, CTRL 1 to 3 in the second, the knob's position, 0 to
       127, in the third, as FUN_80073898 passes them on) all go to the
       model: to the log, to learn the codes, and to know what you have
       just pressed or turned. */
    if (type == page_last_type[page] && type != 1 && (type < 5 || type > 7) && type != 9)
        return 0;
    /* A CTRL knob's stream of positions would log the page's steady
       traffic again after each one. */
    if (type != 9)
        page_last_type[page] = type;
    memset(&d, 0, sizeof d);
    d.kind = EV_PAGE;
    d.tick = device_ticks();
    d.mark = (int32_t)page;
    d.x = type;
    if ((type >= 5 && type <= 7) || type == 9) {
        d.y = message[1];
        d.x1 = message[2];
    }
    /* VALUE turned copying a bank, with where its cursor was. */
    if (type == 7 && d.y == 0 && copy_bank(page)) {
        d.y1 = 2;
        d.ink = copy_side(page);
    }
    /* A bank key, with the pattern store's banks before it. */
    if (type == 5 && d.y >= 0x25 && d.y <= 0x29) {
        d.y1 = 3;
        d.ink = pat_banks();
    }
    /* EXT SOURCE, pressed or let go, with the input's state before. */
    if ((type == 5 || type == 6) && d.y == KEY_EXT_SOURCE && !shift_down) {
        d.y1 = 1;
        d.ink = EXT_ON != 0;
    }
    /* A playback button goes with the current pad's settings before the
       page sees it. With SHIFT, BPM SYNC and GATE set the whole bank and
       say so themselves, BANK A GATE ON, and REVERSE sets the pad mute
       mode; SHIFT and LOOP is the ping pong loop. */
    if (type == 5 && d.y >= KEY_BPM_SYNC && d.y <= KEY_REVERSE && (!shift_down || d.y == 0x1F)) {
        int32_t bank = BANK_NOW, pad = PAD_NOW;
        if (bank >= 0 && bank <= 9 && pad >= 0 && pad <= 15) {
            d.y1 = 1;
            d.x1 = (int16_t)(bank * 16 + pad);
            d.ink = pad_settings(d.x1);
        }
    }
    push(&d);
    return 0;
}

#include "pagestubs.h"

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
        VT(i, SLOT_FRAME) = FRAME_RECT;
    }
    if (icons_hooked)
        for (i = 0; i < ICON_SLOTS; i++)
            ICON_SLOT(i) = ICON_DRAW;
    icons_hooked = 0;
    if (rows_hooked)
        ROW_WORD = ROW_DRAW;
    rows_hooked = 0;
    if (tabs_hooked)
        TAB_WORD = TAB_DRAW;
    tabs_hooked = 0;
    if (pages_hooked)
        for (i = 0; i < PAGES; i++)
            PAGE_TABLE[i] = page_orig[i];
    pages_hooked = 0;
    __asm__ volatile("dsb" ::: "memory");
    screen_hooked = 0;
}

/* The screen as drawn: one item per surface and position. */
#define ITEMS 512
#define ITEM_TEXT DRAW_TEXT
struct item {
    uint32_t surf, lr, site, first_drawn, last_drawn, last_change, erased, draws, seq, drawn_seq;
    int32_t  mark, ink;
    int16_t  x, y;
    uint8_t  used, changed, rapid, task, title, fresh, role, icon_sel, row_sel, framed, own,
             prompted, by_pad, sample;
    char     prev0;                          /* the text's first letter before */
    char     text[ITEM_TEXT];
};
static struct item items[ITEMS];
static int burst[64], nburst;
static uint32_t settle_ticks, batches, evicted, change_seq, draw_seq, page_seq;
static uint32_t last_change, first_pending, last_popup_change;
static const struct item *last_label;
static int pending, wiped, page_pending, muted;
static struct { int16_t action, step; } menu_q[16];
static int menu_n;
/* Pads pressed where a screen asks for pads, waiting to be said once the
   page has made its choice, and when the last was pressed. A TR-REC step
   waits for the sequencer: resolved when it has changed, or given up. */
static struct { int16_t kind; uint8_t resolved; int32_t index, before; uint32_t tick; } picks[8];
#define STEP_WAIT 190u                       /* ticks, 250 ms */
static int npicks;
/* Playback buttons pressed, waiting to be said once the page has set the
   pad. */
static struct { int16_t key, index; int32_t before; } presses[8];
static int npresses;
/* EXT SOURCE pressed or let go since the last batch, and whether the input
   was on before the first of them; likewise VALUE on COPY BANK PAD, and
   where its cursor was. */
static int ext_pending, ext_before = -1, side_pending, side_before = -2;
/* A bank key pressed since the last batch, and the pattern store's banks
   before the first. */
static int pat_pending;
static int32_t pat_before;
static uint32_t last_pick;
static uint8_t pick_seen;
/* Whether the page showing has just been built; the bank the firmware last
   had, and whether it has moved since the last batch. */
static int page_arrived;
static int32_t bank_seen = -1;
static int bank_moved;
/* Whether the last batch had the effect display or its grid showing. */
static int effect_shown;

void screen_mute(int mute)
{
    muted = mute;
}
static struct {
    uint32_t surf, tick;
    int16_t  y;
    uint8_t  sel, valid;
} last_row;
static struct {
    uint32_t surf, tick;
    int16_t  x, y;
    uint8_t  sel, valid;
} last_icon;
/* The tab a tab strip last said was current, when, and whether it changed
   since the last batch. */
static char tab_now[ITEM_TEXT];
static uint32_t tab_tick;
static int tab_changed;

/* The last outline that marks a choice, and the sites of every outline seen
   so far, each logged once. */
static struct {
    uint32_t surf, tick;
    int16_t  x0, y0, x1, y1;
    uint8_t  valid;
} last_frame;
static uint32_t frame_sites[64];
static int nframe_sites;

/* The background colour: white, 0xFFFFFF, on the status bar and on the
   focused item of a menu or grid, 0 or 0x1000000 elsewhere, -1 on the
   pop-up layers. */
#define WHITE 0xFFFFFF
#define STEADY 900u                          /* ticks, 1.2 s */
#define POPUP_LIFE 2250u                     /* ticks, 3 s */
/* Text erased and drawn back within a frame is unchanged: the auto mark
   page draws over its menu every frame and the menu is drawn again the
   next. */
#define ERASED_LIFE 60u                      /* ticks, 80 ms */

/* When a knob, VALUE or CTRL, last moved, and a key was last pressed, as
   the pages were sent them. A page redraws what a knob changed within 40
   ticks of it, one frame, in every log so far. */
#define TURN_WINDOW 60u                      /* ticks, 80 ms */
/* A pad's or a bank key's redraw takes longer: the pattern screen drew
   PLAY-PTN C1 62 ticks after the pad. */
#define PLAY_WINDOW 110u                     /* ticks, 147 ms */
static uint32_t last_turn, last_press, last_pad, last_fx, last_bank_key, last_ctrl_logged;
static uint8_t turn_seen, press_seen, pad_seen, fx_held, fx_seen, bank_key_seen;
static int pad_page;
/* The last key pressed, and what DEL last emptied from a chain's slot. */
#define KEY_DEL 0x1A
static int last_key = -1;
static char chain_removed[ITEM_TEXT];
/* The pattern's state, STOP-PTN C1 or PLAY-PTN C1, is said as soon as it
   settles: one pad draws STOP and then PLAY a frame later. */
static uint32_t state_hold;
static uint8_t state_seen;
static int16_t ctrl_logged = -1;

static int just_after(uint32_t then, int seen, uint32_t now)
{
    return seen && now - then <= TURN_WINDOW;
}

static int played_just_before(uint32_t then, int seen, uint32_t now)
{
    return seen && now - then <= PLAY_WINDOW;
}

/* The page showing, by the last page built; the one built before it; and
   the one showing when the last batch was said. */
static int page_now = 84, page_prev = 84, batch_page = 84;

/* Screens where the pads play, as they do on the main screen, and choose
   nothing: START/END, CHOP and PITCH/SPEED select the sample to edit by
   playing it, and the pattern screen, 60, starts and stops a pattern. A pad
   pressed there is heard, not said, and nor is what it changes, nor what a
   bank key changes there, the bank's letter being said. The screens that
   ask for a pad, recording's, delete, copy, export, import, still name
   it, and so do the pattern screen's DELETE, COPY and EXCHANGE, where the
   pads choose patterns. */
static int pads_play(void)
{
    int32_t mode;

    if (page_now == PAGE_PATTERN) {
        mode = PAT_MODE;
        return mode != PMODE_DELETE && mode != PMODE_COPY && mode != PMODE_EXCHANGE;
    }
    return page_now == 84 || page_now == 83 || page_now == 88 || page_now == 90;
}

/* The draw log: every change of text or background in full; at the end a
   count of each item's redraws, so a screen that redraws continually costs
   a line per item.
   Text leaving the screen is logged when it is taken off the model, not at
   each clear: some screens clear and redraw twenty-five times a second.
   Only what is not yet written is kept, 64 KB of it, a minute of browsing
   files, with 32 KB past it for the counts. */
#define LOG_CAP  (96u * 1024u)
#define SUMMARY  (32u * 1024u)
static char draw_log[LOG_CAP];
static size_t log_len;
static uint32_t log_dropped, page_repeats;

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
            || VT(i, SLOT_FILL) != FILL_RECT || VT(i, SLOT_FRAME) != FRAME_RECT) {
            printf("screen: vtable %08lx reads %08lx %08lx %08lx %08lx, not the drawing calls\n",
                   (unsigned long)vt_base[i], (unsigned long)VT(i, SLOT_TEXT),
                   (unsigned long)VT(i, SLOT_CLEAR), (unsigned long)VT(i, SLOT_FILL),
                   (unsigned long)VT(i, SLOT_FRAME));
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
        VT(i, SLOT_FRAME) = (uint32_t)(uintptr_t)frame_hook | 1u;
    }
    /* The icon words only all together, and only if every one is still the
       icon draw; the screen reader works without them, reading an icon
       menu's every item. */
    icons_hooked = 1;
    for (i = 0; i < ICON_SLOTS; i++)
        if (ICON_SLOT(i) != ICON_DRAW)
            icons_hooked = 0;
    if (icons_hooked)
        for (i = 0; i < ICON_SLOTS; i++)
            ICON_SLOT(i) = (uint32_t)(uintptr_t)icon_hook | 1u;
    else
        printf("screen: an icon slot is not the icon draw; icons not hooked\n");
    if (TAB_WORD == TAB_DRAW) {
        TAB_WORD = (uint32_t)(uintptr_t)tab_hook | 1u;
        tabs_hooked = 1;
    } else {
        printf("screen: the tab strip word is not the tab draw; tabs not hooked\n");
    }
    if (ROW_WORD == ROW_DRAW) {
        ROW_WORD = (uint32_t)(uintptr_t)row_hook | 1u;
        rows_hooked = 1;
    } else {
        printf("screen: the SYSTEM row word is not the row draw; rows not hooked\n");
    }
    /* Every factory must still be a code pointer, or none is hooked. */
    pages_hooked = 1;
    for (i = 0; i < PAGES; i++)
        if ((PAGE_TABLE[i] & 1u) == 0)
            pages_hooked = 0;
    if (pages_hooked)
        for (i = 0; i < PAGES; i++) {
            page_orig[i] = PAGE_TABLE[i];
            PAGE_TABLE[i] = (uint32_t)(uintptr_t)page_stubs[i] | 1u;
        }
    else
        printf("screen: the page table is not all code; pages not hooked\n");
    __asm__ volatile("dsb" ::: "memory");
    log_line("# tick task surface x y background ink len caller site |text|, from %lu ticks,"
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
/* Text erased more than a frame before is gone, whether or not a batch has
   yet taken it off the model: a dialog cancelled and shown again, on a
   screen where nothing else changed between, is drawn afresh. */
static int stale(const struct item *it, uint32_t tick)
{
    return it->erased && tick - it->erased > ERASED_LIFE;
}

static struct item *find(const struct draw *d)
{
    struct item *free_one = NULL, *oldest = NULL;
    int i;

    for (i = 0; i < ITEMS; i++) {
        struct item *it = &items[i];
        if (it->used && it->surf == d->surf && it->x == d->x && it->y == d->y) {
            if (!stale(it, d->tick))
                return it;
            it->used = 0;
        }
    }
    for (i = 0; i < ITEMS; i++) {
        struct item *it = &items[i];
        if (it->used && it->surf == d->surf && it->y == d->y && it->site == d->site
            && d->tick - it->last_drawn > 2u && overlaps(it, d) && !stale(it, d->tick)) {
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

/* The focus: white below the status bar, the label under an icon menu's
   selected icon, a selected settings row, or a choice in its outline. A
   knob's value on white, as the Microscope's timing, is not. */
static int is_lit(const struct item *it)
{
    return (it->mark == WHITE && it->y >= 10 && !it->title && it->role != ROLE_UNLIT
            && it->role != ROLE_HINT && it->role != ROLE_KNOB)
        || it->icon_sel || it->row_sel || it->framed;
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

    while (*t == ' ')
        t++;
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
/* Text as drawn, without the spaces either side, is WORD. */
static int reads(const char *t, const char *word)
{
    size_t n = strlen(word);

    while (*t == ' ')
        t++;
    if (strncmp(t, word, n) != 0)
        return 0;
    for (t += n; *t == ' '; t++)
        ;
    return *t == 0;
}

/* Three capitals or more spaced a letter apart, "D E L", as the big font
   draws a screen's mode. */
static int spaced_capitals(const char *t)
{
    size_t n, i;

    while (*t == ' ')
        t++;
    n = strlen(t);
    while (n > 0 && t[n - 1] == ' ')
        n--;
    if (n < 5 || n % 2 == 0)
        return 0;
    for (i = 0; i < n; i++)
        if (i % 2 == 0 ? t[i] < 'A' || t[i] > 'Z' : t[i] != ' ')
            return 0;
    return 1;
}

/* Bar, dot, beat, dot: "1.1.", "12.4.", and "-1.1." counting in; with
   SHORT, also without the last dot, "1.2", as the pattern chain counts. */
static int bar_beat(const char *t, int short_form)
{
    int dots = 0, digits = 0;

    while (*t == ' ')
        t++;
    if (*t == '-')
        t++;
    for (; *t && *t != ' '; t++) {
        if (*t == '.') {
            if (digits == 0)
                return 0;
            dots++;
            digits = 0;
        } else if (*t >= '0' && *t <= '9') {
            digits++;
        } else {
            return 0;
        }
    }
    return (dots == 2 && digits == 0) || (short_form && dots == 1 && digits > 0);
}

/* What text is by how it reads, where its drawing code has no role. A key's
   legend: a key's name, a colon and a capitalised action or a bracket,
   ENTER:EXE, ENC:ZOOM(2x), C2:LOOP, M:[S] for the MARK button, and the MENU
   box. The power source at the end of the status bar, as FUN_8014A018 draws
   it, DC, USB, BAT or ??? when unknown, which nobody needs to hear; LOW and
   LOW!, the batteries running out, are said. */
static uint8_t role_by_text(const struct draw *d)
{
    static const char *const keys[] = {
        "C1", "C2", "C3", "ENC", "ENT", "ENTER", "EXIT", "SHIFT", "M", "MARK", "ROLL",
        "PAD", "SUB",
    };
    const char *t = d->text;
    size_t i, n;

    if (d->y <= 5 && (reads(t, "DC") || reads(t, "USB") || reads(t, "BAT") || reads(t, "???")))
        return ROLE_IGNORE;
    /* A pattern's bar and beat as it plays, 1.1., 2.4.: a running
       position, never said. The pattern screen and the pattern chain draw
       it and the 2.4 in their corners without the last dot, and no other
       figure of theirs looks so. */
    if (bar_beat(t, page_now == PAGE_PATTERN || page_now == PAGE_CHAIN))
        return ROLE_IGNORE;
    if (reads(t, "MENU"))
        return ROLE_HINT;
    /* The fixed velocity indicator, Fix or Vel, boxed beside the pad on the
       pitch and speed screen: said when it changes, not arriving. */
    if (reads(t, "Vel") || reads(t, "Fix"))
        return ROLE_QUIET;
    /* A screen's mode in its big letters, D E L, R E C, P T N: its title. */
    if (spaced_capitals(t))
        return ROLE_TITLE;
    while (*t == ' ')
        t++;
    for (i = 0; i < sizeof keys / sizeof keys[0]; i++) {
        n = strlen(keys[i]);
        if (strncmp(t, keys[i], n) == 0 && t[n] == ':'
            && ((t[n + 1] >= 'A' && t[n + 1] <= 'Z') || t[n + 1] == '[' || t[n + 1] == '('))
            return ROLE_HINT;
    }
    return ROLE_NONE;
}

static size_t clean(char *out, size_t cap, const char *in);

static int is_alnum_char(char c)
{
    return (c >= '0' && c <= '9') || ((c | 0x20) >= 'a' && (c | 0x20) <= 'z');
}

/* Two texts the same but for a (*) at the end of one. */
static int same_but_star(const char *a, const char *b)
{
    size_t na = strlen(a), nb = strlen(b);

    if (na > nb) {
        const char *t = a;
        size_t n = na;
        a = b;
        b = t;
        na = nb;
        nb = n;
    }
    return nb == na + 3 && strncmp(a, b, na) == 0 && strcmp(b + na, "(*)") == 0;
}

static int take(const struct draw *d)
{
    struct item *it;
    int was_lit, now_lit, text_changed, framed, counts = 0;

    if (d->kind == EV_CLEAR) {
        erase(d->surf, 0, 0, 0, 0, 1, d->tick);
        return 0;
    }
    if (d->kind == EV_FILL) {
        erase(d->surf, d->x, d->y, d->x1, d->y1, 0, d->tick);
        return 0;
    }
    if (d->kind == EV_MENU) {
        if (menu_n < 16) {
            menu_q[menu_n].action = d->x;
            menu_q[menu_n].step = d->y;
            menu_n++;
        }
        return 0;
    }
    if (d->kind == EV_PAGE && d->x == 16 && d->y1 != PICK_NONE) {
        log_line("%lu pick %d, pad %d, index %ld, was %ld, page %ld\n", (unsigned long)d->tick,
                 d->y1, d->y, (long)(int32_t)d->site, (long)d->ink, (long)d->mark);
        if (d->y1 == PICK_STEP)
            log_line("%lu step pad %d note %ld, bar %04x, steps %ld slots %ld last %ld\n",
                     (unsigned long)d->tick, d->y, (long)((int32_t)d->site >> 8),
                     step_mask((int32_t)d->site >> 8), (long)STEP_COUNT, (long)STEP_SLOTS,
                     (long)STEP_LAST);
        if (npicks < (int)(sizeof picks / sizeof picks[0])) {
            picks[npicks].kind = d->y1;
            picks[npicks].index = (int32_t)d->site;
            picks[npicks].before = d->ink;
            picks[npicks].tick = d->tick;
            picks[npicks].resolved = d->y1 != PICK_STEP;
            npicks++;
        }
        last_pick = d->tick;
        pick_seen = 1;
        return 1;
    }
    if (d->kind == EV_PAGE && d->x == 9) {
        /* A CTRL knob sends every position it passes, so only where each
           movement starts goes to the log. */
        if (d->y != ctrl_logged || d->tick - last_ctrl_logged >= STEADY)
            log_line("%lu ctrl %d at %d, page %ld\n", (unsigned long)d->tick, d->y, d->x1,
                     (long)d->mark);
        ctrl_logged = d->y;
        last_ctrl_logged = d->tick;
        last_turn = d->tick;
        turn_seen = 1;
        return 0;
    }
    if (d->kind == EV_PAGE && d->x >= 5 && d->x <= 7) {
        if (d->x == 5) {
            last_press = d->tick;
            press_seen = 1;
            last_key = d->y;
            /* Keys 0 to 15 are the pads, 1 to 16; with an effect's button
               held they choose an effect, which is said, and play nothing. */
            if (d->y >= 0 && d->y < 16 && !fx_held) {
                last_pad = d->tick;
                pad_seen = 1;
                pad_page = (int)d->mark;
            }
            /* The bank keys, A/F to E/J. */
            if (d->y >= 0x25 && d->y <= 0x29) {
                last_bank_key = d->tick;
                bank_key_seen = 1;
            }
            if (d->y1 == 1 && d->y != KEY_EXT_SOURCE
                && npresses < (int)(sizeof presses / sizeof presses[0])) {
                presses[npresses].key = d->y;
                presses[npresses].index = d->x1;
                presses[npresses].before = d->ink;
                npresses++;
                log_line("%lu key down 0x%02x, page %ld, pad %d set %ld\n",
                         (unsigned long)d->tick, (unsigned)(uint16_t)d->y, (long)d->mark, d->x1,
                         (long)d->ink);
                return 1;
            }
        }
        /* The effects' buttons, FILTER+DRIVE to MFX, keys 0x2B to 0x30. */
        if (d->y >= 0x2B && d->y <= 0x30 && (d->x == 5 || d->x == 6)) {
            if (d->x == 5) {
                fx_held |= 1u << (d->y - 0x2B);
                last_fx = d->tick;
                fx_seen = 1;
            } else {
                fx_held &= ~(1u << (d->y - 0x2B));
            }
        }
        if (d->y == KEY_EXT_SOURCE && d->y1 == 1 && (d->x == 5 || d->x == 6)) {
            ext_pending = 1;
            if (ext_before < 0)
                ext_before = d->ink;
        }
        if (d->x == 7 && d->y1 == 2) {
            side_pending = 1;
            if (side_before < -1)
                side_before = d->ink;
        }
        if (d->x == 5 && d->y1 == 3 && !pat_pending) {
            pat_pending = 1;
            pat_before = d->ink;
        }
        if (d->x == 7) {
            last_turn = d->tick;
            turn_seen = 1;
            log_line("%lu knob %d step %d, page %ld\n", (unsigned long)d->tick, d->y, d->x1,
                     (long)d->mark);
        } else {
            log_line("%lu key %s 0x%02x, page %ld\n", (unsigned long)d->tick,
                     d->x == 5 ? "down" : "up", (unsigned)(uint16_t)d->y, (long)d->mark);
        }
        return (d->y == KEY_EXT_SOURCE && d->y1 == 1) || (d->x == 7 && d->y1 == 2)
            || (d->x == 5 && d->y1 == 3);
    }
    if (d->kind == EV_PAGE) {
        /* Some pages are sent the same messages many times a second, the
           file lists 8 and 4 every 53 ms, half of what browsing would log:
           one the page had under 100 ms before is counted, not logged. */
        static uint32_t msg_at[32];
        static int32_t msg_page = -1;
        if (d->mark != msg_page) {
            memset(msg_at, 0, sizeof msg_at);
            msg_page = d->mark;
        }
        if (d->x > 1 && d->x < 32 && msg_at[d->x] != 0 && d->tick - msg_at[d->x] < 75u)
            page_repeats++;
        else
            log_line("%lu page %ld message %d\n", (unsigned long)d->tick, (long)d->mark, d->x);
        if (d->x >= 0 && d->x < 32)
            msg_at[d->x] = d->tick;
        if (d->x != 1)
            return 0;
        if ((int)d->mark != page_now)
            page_prev = page_now;
        page_now = (int)d->mark;
        page_pending = 1;
        page_seq = draw_seq;
        return 1;
    }
    if (d->kind == EV_TAB) {
        tab_tick = d->tick;
        if (strcmp(tab_now, d->text) == 0)
            return 0;
        memcpy(tab_now, d->text, ITEM_TEXT);
        tab_changed = 1;
        log_line("%lu tab %ld |%s|\n", (unsigned long)d->tick, (long)d->mark, d->text);
        return 1;
    }
    if (d->kind == EV_ROW) {
        last_row.surf = d->surf;
        last_row.y = d->y;
        last_row.sel = d->mark != 0;
        last_row.tick = d->tick;
        last_row.valid = 1;
        return 0;
    }
    if (d->kind == EV_FRAME) {
        int k;
        for (k = 0; k < nframe_sites && frame_sites[k] != d->site; k++)
            ;
        if (k == nframe_sites && k < (int)(sizeof frame_sites / sizeof frame_sites[0])) {
            frame_sites[nframe_sites++] = d->site;
            log_line("%lu outline %08lx %d %d %d %d %08lx\n", (unsigned long)d->tick,
                     (unsigned long)d->surf, d->x, d->y, d->x1, d->y1, (unsigned long)d->site);
        }
        if (role_of(d->site) == ROLE_CHOICE) {
            last_frame.surf = d->surf;
            last_frame.x0 = d->x;
            last_frame.y0 = d->y;
            last_frame.x1 = d->x1;
            last_frame.y1 = d->y1;
            last_frame.tick = d->tick;
            last_frame.valid = 1;
        }
        return 0;
    }
    if (d->kind == EV_ICON) {
        if (d->mark && (last_icon.surf != d->surf || last_icon.x != d->x
                        || last_icon.y != d->y || !last_icon.sel))
            log_line("%lu icon %08lx %d %d %08lx |%s| selected\n", (unsigned long)d->tick,
                     (unsigned long)d->surf, d->x, d->y, (unsigned long)d->site, d->text);
        last_icon.surf = d->surf;
        last_icon.x = d->x;
        last_icon.y = d->y;
        last_icon.sel = d->mark != 0;
        last_icon.tick = d->tick;
        last_icon.valid = 1;
        return 0;
    }
    it = find(d);
    was_lit = is_lit(it);
    /* An icon menu draws each label straight after its icon, below it and
       in its column; each draw says afresh whether the label is under the
       selected icon. The icons go on the display's global surface and the
       labels on the page's own, so only the order and the position link
       them. */
    /* A settings row's name and value both take its selected state. */
    it->row_sel = 0;
    if (last_row.valid && d->tick - last_row.tick <= 2 && d->surf == last_row.surf
        && d->y >= last_row.y - 1 && d->y <= last_row.y + 1)
        it->row_sel = last_row.sel;
    it->icon_sel = 0;
    if (last_icon.valid && d->tick - last_icon.tick <= 2) {
        int w = 4 * (int)(d->len ? d->len : strlen(d->text));
        if (d->y > last_icon.y && d->y <= last_icon.y + 32
            && d->x < last_icon.x + 28 && d->x + w > last_icon.x - 8)
            it->icon_sel = (uint8_t)last_icon.sel;
        last_icon.valid = 0;
    }
    /* A choice's outline is drawn just before its text, which starts inside
       it; a centred string may start a pixel or two left of it. */
    framed = last_frame.valid && d->tick - last_frame.tick <= 2 && d->surf == last_frame.surf
        && d->x >= last_frame.x0 - 3 && d->x <= last_frame.x1
        && d->y >= last_frame.y0 && d->y <= last_frame.y1;
    if (framed != it->framed)
        log_line("%lu %s %08lx %d %d |%s|\n", (unsigned long)d->tick,
                 framed ? "chosen" : "unchosen", (unsigned long)d->surf, d->x, d->y, d->text);
    it->framed = (uint8_t)framed;
    it->erased = 0;
    it->draws++;
    it->last_drawn = d->tick;
    it->drawn_seq = ++draw_seq;
    it->lr = d->lr;
    it->site = d->site;
    it->task = d->task;
    it->role = role_of(d->site);
    if (it->role == ROLE_NONE)
        it->role = role_by_text(d);
    /* The pattern screen's big fields: in its corners the pattern and a
       figure, 2.4, which the state names, never said; in the middle the
       pattern chosen, C 1, which the state names too, said only when it
       changes. */
    if (page_now == PAGE_PATTERN && d->site == BIG_TEXT && it->role == ROLE_NONE)
        it->role = d->y >= 50 ? ROLE_IGNORE : padlike(d->text) ? ROLE_QUIET : ROLE_NONE;
    it->title = it->role == ROLE_TITLE;
    text_changed = strcmp(it->text, d->text) != 0;
    it->sample = d->sample;
    /* The chain's heading gaining (*) once the chain is changed is not
       news. */
    if (text_changed && d->site == CHAIN_TITLE && same_but_star(it->text, d->text)) {
        memcpy(it->text, d->text, ITEM_TEXT);
        text_changed = 0;
    }
    if (text_changed || it->mark != d->mark || it->ink != d->ink)
        log_line("%lu %u %08lx %d %d %ld %ld %u %08lx %08lx |%s|\n", (unsigned long)d->tick,
                 (unsigned)d->task, (unsigned long)d->surf, d->x, d->y, (long)d->mark,
                 (long)d->ink, (unsigned)d->len, (unsigned long)d->lr, (unsigned long)d->site,
                 d->text);
    it->mark = d->mark;
    it->ink = d->ink;
    now_lit = is_lit(it);
    if (text_changed) {
        int turned = just_after(last_turn, turn_seen, d->tick);

        it->prompted = (uint8_t)(turned || just_after(last_press, press_seen, d->tick));
        /* What a pad or a bank key changes where the pads play, and the
           tempo a bank key brings on the pattern screen in every mode, is
           not said; what a pad chooses is. */
        it->by_pad = (uint8_t)(it->role != ROLE_STATE && it->role != ROLE_ANSWER
                               && ((pads_play() && played_just_before(last_pad, pad_seen, d->tick))
                                   || ((pads_play() || page_now == PAGE_PATTERN)
                                       && played_just_before(last_bank_key, bank_key_seen,
                                                             d->tick))));
        /* A chain's slot emptied by DEL: what it held, to be said. */
        if ((d->site == CHAIN_SLOT || d->site == CHAIN_SLOT_LIT) && last_key == KEY_DEL
            && just_after(last_press, press_seen, d->tick) && !page_pending) {
            char t[ITEM_TEXT];
            if (clean(t, sizeof t, d->text) == 0 && clean(t, sizeof t, it->text) > 0)
                memcpy(chain_removed, it->text, ITEM_TEXT);
        }
        if (it->role == ROLE_STATE) {
            state_hold = d->tick + 75u;
            state_seen = 1;
        }
        /* Text that changes with nothing just pressed or turned changes on
           its own, as a meter, a clock or the sequencer's bar does, and is
           never taken for a value turned until the screen changes, however
           busy the knobs are meanwhile. */
        if (it->last_change != 0 && !it->prompted)
            it->own = 1;
        /* A first change is said at once. One that follows another within
           STEADY neither interrupts nor is said until the item has held still
           that long: a meter or a clock that never stops is not heard at all,
           and an effect being played is heard where it stops. A value just
           turned or changed by a key is said at every step, as the focus is,
           the pop-ups a key brings among them, COUNT-IN 2MEAS, COUNT-IN WAIT.
           A pad, and the main screen's status, change only when something
           is done, however quickly, so they are dealt with at once, and so is
           what a copy draws in answer to them. */
        if ((it->prompted && !it->own && it->role != ROLE_EFFECT) || it->role == ROLE_STATE
            || it->role == ROLE_COPY_FROM || it->role == ROLE_COPY_TO || it->role == ROLE_ANSWER) {
            it->rapid = 0;
        } else if (it->last_change != 0 && d->tick - it->last_change < STEADY
                   && !padlike(d->text) && it->role != ROLE_MAIN && it->role != ROLE_PAD) {
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
        it->prompted = (uint8_t)(just_after(last_turn, turn_seen, d->tick)
                                 || just_after(last_press, press_seen, d->tick));
        it->seq = ++change_seq;
        counts = 1;
    }
    /* A page just built draws itself unchanged on a layer it left before:
       everything it draws counts, so the batch waits for the whole of it. */
    if (page_pending)
        counts = 1;
    if (screen_mode == SCREEN_ALL) {
        int k = (int)(it - items);
        if (nburst < (int)(sizeof burst / sizeof burst[0])
            && (nburst == 0 || burst[nburst - 1] != k))
            burst[nburst++] = k;
        counts = 1;
    }
    return counts;
}

/* A string worth saying: runs of spaces closed up, and a space before a
   colon dropped, since OpenEVV says "colon" for MODE : TR-REC; a space put
   before a bracket a word runs into, since it spells ZOOM(2x) out a letter
   at a time, and a hyphen after a bracket read as a pause, not "dash",
   as in PROJECT(INT)-CURR; the ends
   trimmed, a trailing ".." of truncation dropped, text spaced out a letter
   at a time ("9 4", "R E C") closed up, a bar of so many, BAR:1/2, as 1 of
   2, which OpenEVV would read as a half, and at least one letter or digit
   in it. */
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
        if (space && c != ':' && n + 2 < cap)
            out[n++] = ' ';
        space = 0;
        if (c == '(' && n > 0 && n + 2 < cap
            && (is_alnum_char(out[n - 1]) || out[n - 1] == '.'))
            out[n++] = ' ';
        if (c == '-' && n > 0 && out[n - 1] == ')' && n + 3 < cap) {
            out[n++] = ',';
            space = 1;
            continue;
        }
        if ((c >= '0' && c <= '9') || ((c | 0x20) >= 'a' && (c | 0x20) <= 'z'))
            alnum = 1;
        out[n++] = (char)c;
    }
    out[n] = 0;
    while (n >= 3 && out[n - 1] == '.' && out[n - 2] == '.')
        out[n -= 2] = 0;
    while (n > 0 && out[n - 1] == ' ')
        out[--n] = 0;
    {
        char *b = strstr(out, "BAR:"), *d;
        if (b != NULL) {
            for (d = b + 4; *d == ' '; d++)
                ;
            if (*d >= '0' && *d <= '9') {
                while (*d >= '0' && *d <= '9')
                    d++;
                if (*d == '/' && d[1] >= '0' && d[1] <= '9' && n + 4 < cap) {
                    memmove(d + 4, d + 1, strlen(d + 1) + 1);
                    memcpy(d, " of ", 4);
                    n += 3;
                }
            }
        }
    }
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

/* The effects by their full names, from the reference manual's MFX list,
   input effects included. */
static const char *const effect_names[] = {
    "Filter+Drive", "Resonator", "Sync Delay", "Isolator", "DJFX Looper", "Scatter",
    "Downer", "Ha-Dou", "Ko-Da-Ma", "Zan-Zou", "To-Gu-Ro", "SBF", "Stopper", "Tape Echo",
    "TimeCtrlDly", "Super Filter", "WrmSaturator", "303 VinylSim", "404 VinylSim",
    "Cassette Sim", "Lo-fi", "Reverb", "Chorus", "JUNO Chorus", "Flanger", "Phaser", "Wah",
    "Slicer", "Tremolo/Pan", "Chromatic PS", "Hyper-Reso", "Ring Mod", "Crusher",
    "Overdrive", "Distortion", "Equalizer", "Compressor", "SX Reverb", "SX Delay",
    "Cloud Delay", "Back Spin", "DJFX Delay", "Auto Pitch", "Vocoder", "Harmony",
    "Gt Amp Sim", "Bypass",
};

/* A name cut short ("Crush..") as its full name ("Crusher"): the one also on
   the screen, as the title over the effects grid, else the one effect whose
   name it begins, since a grid page showing none selected has no title.
   "DJFX .." begins two and stays as it is. */
static const char *full_text(const struct item *it)
{
    size_t n = strlen(it->text), k;
    const char *found = NULL;
    int i;

    /* The chain's heading without the (*) a change adds, which OpenEVV
       reads as "asterisk". */
    if (it->site == CHAIN_TITLE && n > 3 && strcmp(it->text + n - 3, "(*)") == 0) {
        static char bare[ITEM_TEXT];
        memcpy(bare, it->text, n - 3);
        bare[n - 3] = 0;
        return bare;
    }
    /* A file list's folder "..", by what it does: the manual has no name
       for it, and this one was chosen on 5 October 2026. */
    if (it->site == SCROLL_LIST && strcmp(it->text, "..") == 0)
        return "parent folder";
    if (n < 3 || it->text[n - 1] != '.' || it->text[n - 2] != '.')
        return it->text;
    n -= 2;
    for (i = 0; i < ITEMS; i++) {
        const struct item *o = &items[i];
        if (live(o) && o != it && o->surf == it->surf && strlen(o->text) > n
            && strncmp(o->text, it->text, n) == 0 && o->text[n] != '.')
            return o->text;
    }
    while (n > 0 && it->text[n - 1] == ' ')
        n--;
    for (k = 0; n > 0 && k < sizeof effect_names / sizeof effect_names[0]; k++)
        if (strncmp(effect_names[k], it->text, n) == 0) {
            if (found != NULL)
                return it->text;
            found = effect_names[k];
        }
    return found != NULL ? found : it->text;
}

/* The batch being built: phrases one after another, each ended by a zero. */
static char *b_out;
static size_t b_cap, b_used;
static int b_count;
static uint32_t b_now, b_prev, screen_epoch;

static int in_batch(const char *text)
{
    const char *p = b_out;
    int i;

    for (i = 0; i < b_count; i++, p += strlen(p) + 1)
        if (strcmp(p, text) == 0)
            return 1;
    return 0;
}

/* Roland's abbreviations, said in full, as he approved them on 27 September
   2026 from every screen logged and every key legend in the firmware, in
   the reference manual's own words: whole words only, capitals as written,
   and a number stuck to one set apart, "2MEAS" as "2 measures". What the
   manual never spells out, Ring Mod, Sim, TS type, stays as Roland writes
   it. The engine's own dictionary stays off, since SD is not South Dakota.
   Bn, the pattern screen's DELETE BANK, and Ptn, TR-REC's, he added on 29
   September. */
static const struct { const char *from, *to; } spelt_phrases[] = {
    { "PC Rx", "program change receive" },
    { "MANU-F", "manual-F" },
    { "X-FADE", "crossfade" },
    { "TimeCtrlDly", "time control delay" },
    { "WrmSaturator", "warm saturator" },
    { "Chromatic PS", "chromatic pitch shifter" },
    { "Gt Amp Sim", "guitar amp Sim" },
    { "Hyper-Reso", "hyper resonator" },
    { "VinylSim", "Vinyl Sim" },
};
static const struct { const char *from, *to; } spelt_words[] = {
    { "ENC", "VALUE" }, { "EXE", "execute" }, { "SEL", "select" }, { "MOV", "move" },
    { "CHG", "change" }, { "FLD", "folder" }, { "PTN", "pattern" }, { "SMPL", "sample" },
    { "PROJ", "project" }, { "TOT", "total" }, { "DEST", "destination" },
    { "CURR", "current" }, { "INT", "internal" }, { "EXT", "external" },
    { "SRC", "source" }, { "DEL", "delete" }, { "QTZ", "quantize" }, { "MANU", "manual" },
    { "SBS", "skip-back sampling" }, { "SEQ", "sequencer" }, { "VELO", "velocity" },
    { "Vel", "velocity" }, { "Fix", "fixed" }, { "Retrig", "retrigger" },
    { "Chrom", "chromatic" }, { "Lin", "linear" }, { "Def", "default" },
    { "Init", "initialize" }, { "Dtct", "detect" }, { "Rng", "range" },
    { "Scrn", "screen" }, { "Disp", "display" }, { "Trig", "trigger" },
    { "Sens", "sensitivity" }, { "METRO", "metronome" }, { "XFADE", "crossfade" },
    { "EFX", "effects" }, { "FX", "effects" }, { "FLT", "filter" }, { "HPF", "high pass" },
    { "LPF", "low pass" }, { "BPF", "band pass" }, { "COMP", "compression" },
    { "FLUT", "flutter" }, { "Mst", "master" }, { "Phn", "phones" }, { "Rx", "receive" },
    { "SBF", "sideband filter" }, { "msec", "milliseconds" }, { "Hz", "hertz" },
    { "kHz", "kilohertz" }, { "dB", "decibels" }, { "SEMI", "semitones" }, { "Ptn", "pattern" },
    { "Bn", "bank" },
};
/* Before a number: C1 for the CTRL 1 knob in a key's legend, CH1, DECK1. */
static const struct { const char *from, *to; } spelt_numbered[] = {
    { "C", "control" }, { "CTR", "control" }, { "CTRL", "control" }, { "CH", "channel" },
    { "DECK", "deck" },
};

static int is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static int is_letter(char c)
{
    return (c | 0x20) >= 'a' && (c | 0x20) <= 'z';
}

/* A word in full, by what is around it: M before a colon is the MARK
   button; (M) and (C) are auto mark's marker and cursor; [S] and [E] the
   start and end the MARK button sets; TS a time signature before 4/4, and
   otherwise, as in DJ Mode TS type, which the manual never spells out, as
   written; MEAS one measure after a 1. */
static const char *spelt(const char *w, size_t n, char before, char after, char after2, int one)
{
    size_t i;

    if (n == 1) {
        if (w[0] == 'M' && after == ':')
            return "mark";
        if (before == '(' && after == ')')
            return w[0] == 'M' ? "marker" : w[0] == 'C' ? "cursor" : NULL;
        if (before == '[' && after == ']')
            return w[0] == 'S' ? "start" : w[0] == 'E' ? "end" : NULL;
        return NULL;
    }
    if (n == 2 && strncmp(w, "TS", 2) == 0)
        return after == ':' && is_digit(after2) ? "time signature" : NULL;
    if (n == 4 && strncmp(w, "MEAS", 4) == 0)
        return one ? "measure" : "measures";
    for (i = 0; i < sizeof spelt_words / sizeof spelt_words[0]; i++)
        if (strlen(spelt_words[i].from) == n && strncmp(w, spelt_words[i].from, n) == 0)
            return spelt_words[i].to;
    return NULL;
}

static void put(char *out, size_t cap, size_t *n, const char *s, size_t k)
{
    if (*n + k + 1 > cap)
        k = cap - *n - 1;
    memcpy(out + *n, s, k);
    *n += k;
    out[*n] = 0;
}

static size_t spell_out(char *out, size_t cap, const char *in)
{
    size_t n = 0, i = 0, len = strlen(in), k;
    int one = 0;

    out[0] = 0;
    while (i < len && n + 1 < cap) {
        size_t t0, a, d;
        const char *f;
        int matched = 0;
        char before, after, after2;

        if (i == 0 || !(is_letter(in[i - 1]) || is_digit(in[i - 1])))
            for (k = 0; k < sizeof spelt_phrases / sizeof spelt_phrases[0]; k++) {
                size_t m = strlen(spelt_phrases[k].from);
                if (strncmp(in + i, spelt_phrases[k].from, m) == 0 && !is_letter(in[i + m])
                    && !is_digit(in[i + m])) {
                    put(out, cap, &n, spelt_phrases[k].to, strlen(spelt_phrases[k].to));
                    i += m;
                    matched = 1;
                    one = 0;
                    break;
                }
            }
        if (matched)
            continue;
        if (!is_letter(in[i]) && !is_digit(in[i])) {
            put(out, cap, &n, in + i, 1);
            i++;
            continue;
        }
        t0 = i;
        while (i < len && (is_letter(in[i]) || is_digit(in[i])))
            i++;
        before = t0 ? in[t0 - 1] : 0;
        after = in[i];
        after2 = after ? in[i + 1] : 0;
        for (a = 0; t0 + a < i && is_letter(in[t0 + a]); a++)
            ;
        for (d = 0; t0 + d < i && is_digit(in[t0 + d]); d++)
            ;
        f = NULL;
        if (a == i - t0) {
            f = spelt(in + t0, a, before, after, after2, one);
            if (f != NULL)
                put(out, cap, &n, f, strlen(f));
            /* The dot of an abbreviation said in full, SEL. PROJECT, which
               would stop the phrase. */
            if (f != NULL && in[i] == '.' && !is_digit(in[i + 1]))
                i++;
        } else if (a > 0) {
            for (d = a; t0 + d < i && is_digit(in[t0 + d]); d++)
                ;
            /* C1 is the CTRL 1 knob only in a key's legend, C1:START or
               [C2]; elsewhere it is the pattern or the pad C1. */
            for (k = 0; d == i - t0 && k < sizeof spelt_numbered / sizeof spelt_numbered[0]; k++)
                if (strlen(spelt_numbered[k].from) == a
                    && strncmp(in + t0, spelt_numbered[k].from, a) == 0
                    && (a > 1 || after == ':' || (before == '[' && after == ']'))) {
                    f = spelt_numbered[k].to;
                    put(out, cap, &n, f, strlen(f));
                    put(out, cap, &n, " ", 1);
                    put(out, cap, &n, in + t0 + a, i - t0 - a);
                    break;
                }
        } else if (d > 0) {
            for (a = d; t0 + a < i && is_letter(in[t0 + a]); a++)
                ;
            if (a == i - t0 && a > d)
                f = spelt(in + t0 + d, a - d, 0, after, after2, d == 1 && in[t0] == '1');
            if (f != NULL) {
                put(out, cap, &n, in + t0, d);
                put(out, cap, &n, " ", 1);
                put(out, cap, &n, f, strlen(f));
            }
        }
        if (f == NULL)
            put(out, cap, &n, in + t0, i - t0);
        one = i - t0 == 1 && in[t0] == '1';
    }
    return n;
}

#define WIDE 160

/* A colon between two things, "VALUE:ZOOM(2x)", is read aloud as "colon";
   a space after it makes OpenEVV pause instead. Not between two digits,
   where it may be a time. */
static size_t space_colons(char *s, size_t n, size_t cap)
{
    size_t i;

    for (i = 1; i + 1 < n && n + 1 < cap; i++)
        if (s[i] == ':' && s[i + 1] != ' ' && s[i - 1] != ' '
            && !(is_digit(s[i - 1]) && is_digit(s[i + 1]))) {
            memmove(s + i + 2, s + i + 1, n - i);
            s[i + 1] = ' ';
            n++;
        }
    return n;
}

/* Adds a phrase made of up to three texts. Answers 1 if added, 2 if the
   batch already says it, 0 if it says nothing or there is no room. */
static int say3(const char *a, const char *b, const char *c)
{
    char buf[3 * WIDE], part[ITEM_TEXT], wide[WIDE];
    size_t n = 0;
    const char *src[3] = { a, b, c };
    int i;

    buf[0] = 0;
    for (i = 0; i < 3; i++) {
        size_t k;
        if (src[i] == NULL)
            continue;
        if (clean(part, sizeof part, src[i]) == 0)
            continue;
        k = spell_out(wide, sizeof wide, part);
        if (k == 0)
            continue;
        k = space_colons(wide, k, sizeof wide);
        if (n > 0)
            buf[n++] = ' ';
        memcpy(buf + n, wide, k + 1);
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

/* The file lists' line naming where a sample will go, DEST:PRESS PAD, is
   said when it appears and when it changes. Going into a folder and
   scrolling a page redraw it with the list, which reads as a new screen;
   leaving the list for the import menu clears what was said. The same line
   shows an import's progress, WAIT: IMPORTING 70%, its numbers in an order
   that tells nothing, so it is said once, without them. */
#define DEST_LINE 0x80161895u
static char dest_said[ITEM_TEXT];

static int say(const struct item *it)
{
    char t[ITEM_TEXT];
    size_t n;

    if (it->site != DEST_LINE)
        return say3(full_text(it), NULL, NULL);
    memcpy(t, it->text, ITEM_TEXT);
    n = strlen(t);
    while (n > 0 && t[n - 1] == ' ')
        n--;
    if (n > 0 && t[n - 1] == '%') {
        n--;
        while (n > 0 && t[n - 1] >= '0' && t[n - 1] <= '9')
            n--;
        while (n > 0 && t[n - 1] == ' ')
            n--;
    }
    t[n] = 0;
    if (strcmp(t, dest_said) == 0)
        return 0;
    memcpy(dest_said, t, ITEM_TEXT);
    return say3(t, NULL, NULL);
}

/* A title of one word the batch has already said, as the big S E L under
   SEL. PROJECT(INT)-CURR:02(INT): "select" once. */
static int title_said(const struct item *it)
{
    char part[ITEM_TEXT], word[WIDE];
    const char *p = b_out;
    size_t n;
    int i;

    if (clean(part, sizeof part, full_text(it)) == 0)
        return 0;
    n = spell_out(word, sizeof word, part);
    if (n == 0 || strchr(word, ' ') != NULL)
        return 0;
    for (i = 0; i < b_count; i++, p += strlen(p) + 1) {
        const char *q = p;
        while ((q = strstr(q, word)) != NULL) {
            if ((q == p || !is_letter(q[-1])) && !is_letter(q[n]))
                return 1;
            q += n;
        }
    }
    return 0;
}

static const struct item *row_label(const struct item *v);

/* The focus: a settings row as its name and value together, once. */
static int say_focus(const struct item *it)
{
    const struct item *l = row_label(it);
    int i;

    if (l != NULL && l->row_sel && it->row_sel)
        return 2;
    for (i = 0; i < ITEMS; i++) {
        const struct item *v = &items[i];
        if (v != it && live(v) && v->row_sel && row_label(v) == it) {
            last_label = it;
            return say3(it->text, v->text, NULL);
        }
    }
    return say(it);
}

/* A pad, "A-13" or "A 1", as "A 13" or "A 1", written out directly:
   cleaning would close up "B 1" the way it closes up the big letter-spaced
   digits. */
static int say_pad(const struct item *it)
{
    char t[ITEM_TEXT];
    const char *p = it->text;
    size_t n;

    /* The pad field also shows what is not a pad, EXT for EXT SOURCE. */
    if (!padlike(p))
        return say(it);
    while (*p == ' ')
        p++;
    n = strlen(p);
    if (n + 2 > sizeof t)
        return 0;
    if (p[1] == '-' || p[1] == ' ') {
        memcpy(t, p, n + 1);
    } else {
        t[0] = p[0];
        memcpy(t + 1, p, n + 1);
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

/* A copy's or an exchange's pads either side of >> or <>, A1 >> B3, which
   OpenEVV reads as "greater than" twice: the side a pad has just chosen,
   in the manual's words as COPY BANK has them, "source A1", then
   "destination B3", and for an exchange the pad alone; -- is nothing
   chosen. The sides as last said, to know which changed; a new screen
   starts with none. */
static char pair_said[2][ITEM_TEXT];

static void pair_side(char *out, const char *from, const char *to)
{
    size_t n;

    while (from < to && *from == ' ')
        from++;
    while (to > from && to[-1] == ' ')
        to--;
    n = (size_t)(to - from) < ITEM_TEXT - 1 ? (size_t)(to - from) : ITEM_TEXT - 1;
    memcpy(out, from, n);
    out[n] = 0;
    if (strcmp(out, "--") == 0 || strcmp(out, "-") == 0)
        out[0] = 0;
}

static int say_pair(const struct item *it)
{
    const char *copy = strstr(it->text, ">>"), *swap = strstr(it->text, "<>");
    const char *mark = copy != NULL ? copy : swap;
    char side[2][ITEM_TEXT];
    int k;

    if (mark == NULL)
        return 0;
    pair_side(side[0], it->text, mark);
    pair_side(side[1], mark + 2, it->text + strlen(it->text));
    for (k = 0; k < 2; k++) {
        if (side[k][0] && strcmp(side[k], pair_said[k]) != 0) {
            if (copy != NULL)
                say3(k == 0 ? "source" : "destination", side[k], NULL);
            else
                say3(side[k], NULL, NULL);
        }
        memcpy(pair_said[k], side[k], ITEM_TEXT);
    }
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

/* Text that can label a value: not a title, the status bar, a tab, a
   highlighted menu item or a chosen one. A settings row's name labels its value whether or
   not the row is selected. */
static int page_number(const struct item *it);

static int plain(const struct item *it)
{
    return !is_title(it) && !is_status(it) && it->role != ROLE_TABS && it->role != ROLE_HINT
        && !it->icon_sel && !it->framed && !(it->mark == WHITE && it->y >= 10)
        && !page_number(it);
}

/* A value's label is plain text drawn by other code, and drawn since the
   current screen began, so that what a screen left underneath, as the main
   screen's big BPM under the SD card menu, labels nothing: on its own line to
   its left and on another background, as the SYSTEM page sets "Edit Knob
   Mode" beside "Direct", or else above it in its column, as the parameter
   pages set CUTOFF fifteen pixels over 827 and the pattern settings LENGTH
   thirty over 2 Bars. Text drawn by the same code is a neighbour, as a
   grid's cells are, and so is text beside it on the same background, as a
   menu's items are; neither is a label. */
/* An effect's value is named by the effect page's own names alone: the page
   is drawn over the main screen, whose big tempo, redrawn every frame on the
   same surface, sits between FEEDBACK and its value. */
static int may_name(const struct item *o, const struct item *v)
{
    return v->role != ROLE_EFFECT || o->role == ROLE_FXLABEL;
}

static const struct item *row_label(const struct item *v)
{
    const struct item *best = NULL;
    int i;

    for (i = 0; i < ITEMS; i++) {
        const struct item *o = &items[i];
        if (!live(o) || o == v || o->surf != v->surf || !plain(o) || o->site == v->site
            || o->mark == v->mark || o->x >= v->x || o->y - v->y > 2 || v->y - o->y > 2
            || (int32_t)(o->last_drawn - screen_epoch) < 0 || !may_name(o, v))
            continue;
        if (best == NULL || o->x > best->x)
            best = o;
    }
    return best;
}

/* Text that already names a value beside it on its own line, as the
   pattern settings' QTZ: names GRID 16, names nothing below it. The status
   bar has no names, whatever sits on its line. */
static int labels_its_row(const struct item *o)
{
    int i;

    for (i = 0; i < ITEMS; i++) {
        const struct item *w = &items[i];
        if (live(w) && w != o && w->surf == o->surf && w->x > o->x && w->y - o->y <= 2
            && o->y - w->y <= 2 && !is_status(w) && row_label(w) == o)
            return 1;
    }
    return 0;
}

/* Text with nothing that could name it just above it in its column. */
static int column_top(const struct item *o)
{
    int i;

    for (i = 0; i < ITEMS; i++) {
        const struct item *p = &items[i];
        if (live(p) && p != o && p->surf == o->surf && plain(p) && p->y < o->y - 2
            && o->y - p->y <= 20 && horizontally_near(o, p))
            return 0;
    }
    return 1;
}

/* Nothing that could be named between a heading and the value under it. */
static int column_clear(const struct item *o, const struct item *v)
{
    int i;

    for (i = 0; i < ITEMS; i++) {
        const struct item *p = &items[i];
        if (live(p) && p != o && p != v && p->surf == v->surf && plain(p) && p->y > o->y + 2
            && p->y < v->y - 2 && horizontally_near(v, p))
            return 0;
    }
    return 1;
}

/* Two strings centred on one another, at four pixels a character. */
static int centred(const struct item *a, const struct item *b)
{
    int d = 2 * (a->x - b->x) + 4 * ((int)text_len(a) - (int)text_len(b));

    return d >= -12 && d <= 12;
}

static const struct item *column_label(const struct item *v, int reach, int heading)
{
    const struct item *best = NULL;
    int i;

    for (i = 0; i < ITEMS; i++) {
        const struct item *o = &items[i];
        if (!live(o) || o == v || o->surf != v->surf || !plain(o) || o->site == v->site
            || o->y >= v->y - 2 || v->y - o->y > reach || !horizontally_near(v, o)
            || (int32_t)(o->last_drawn - screen_epoch) < 0 || labels_its_row(o)
            || !may_name(o, v) || !column_top(o)
            || (heading && (!centred(o, v) || !column_clear(o, v))))
            continue;
        if (best == NULL || o->y > best->y)
            best = o;
    }
    return best;
}

/* Above in its column, the nearest text within twenty pixels, else text
   within thirty-two centred over it, and either way text heading its
   column: a value names nothing below it, as the effect page's does not
   name the effect nor the pad settings' SPEED 100.0% its BPM:90.00, and nor
   does a prompt name what sits far under it, as PLEASE SELECT SMPL does not
   name ENTER:EXE. */
/* Values whose names are not beside them: the Microscope's, under the
   knobs' legends in its status bar, C1:ITEM, C2:PITCH and C3:VELO, with the
   note's step line between. The pitch names itself, CHROM:-8, and the
   timing VALUE moves has no name on the screen. */
static const struct { uint32_t site; const char *label; } fixed_labels[] = {
    { MICRO_TIMING, "" }, { MICRO_PITCH, "" }, { MICRO_VELOCITY, "VELO" },
};
static uint32_t last_fixed;

static int fixed_label(const struct item *v, const char **label)
{
    unsigned i;

    for (i = 0; i < sizeof fixed_labels / sizeof fixed_labels[0]; i++)
        if (fixed_labels[i].site == v->site) {
            *label = fixed_labels[i].label;
            return 1;
        }
    return 0;
}

static const struct item *label_of(const struct item *v)
{
    const struct item *best;
    const char *fixed;

    if (fixed_label(v, &fixed))
        return NULL;
    best = row_label(v);

    if (best == NULL)
        best = column_label(v, 20, 0);
    if (best == NULL)
        best = column_label(v, 32, 1);
    return best;
}

/* A value's unit is short text just below it in its column, "Hz" under
   827, "Years" under Cassette Sim's AGE; only a value with a label has one,
   and neither a value named on its own row, the next setting down, nor a
   pad or pattern, as the pattern settings show A-1 under GRID 16, nor the
   pad and pattern settings' own bottom line, VINYL under PITCH, is a
   unit. */
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
        if (clean(t, sizeof t, o->text) == 0 || strlen(t) > 5 || padlike(o->text)
            || o->role == ROLE_KNOB || row_label(o) != NULL)
            continue;
        if (best == NULL || o->y < best->y)
            best = o;
    }
    return best;
}

/* A value's unit as said: none when it only repeats the label, as the
   pattern settings' BPM does under its tempo. */
static const char *unit_said(const struct item *v, const struct item *l)
{
    const struct item *u = unit_of(v);
    char a[ITEM_TEXT], b[ITEM_TEXT];

    if (u == NULL)
        return NULL;
    if (l != NULL && clean(a, sizeof a, u->text) > 0 && clean(b, sizeof b, l->text) > 0
        && strcmp(a, b) == 0)
        return NULL;
    return u->text;
}

/* A value with its label the first time it is heard, alone after that. */
static int say_value(const struct item *v)
{
    const struct item *l = label_of(v);
    const char *fixed;

    if (fixed_label(v, &fixed)) {
        if (*fixed && last_fixed != v->site) {
            last_fixed = v->site;
            last_label = NULL;
            return say3(fixed, full_text(v), NULL);
        }
        return say3(full_text(v), NULL, NULL);
    }
    if (l != NULL && l != last_label) {
        last_label = l;
        last_fixed = 0;
        return say3(l->text, full_text(v), unit_said(v, l));
    }
    return say3(full_text(v), NULL, NULL);
}

/* A title drawn since the last batch. A title's layer may never be wiped,
   and UTILITY MENU read itself before every screen after it. */
static int want_title(const struct item *it)
{
    return is_title(it) && (int32_t)(it->last_drawn - b_prev) > 0;
}
/* The focus drawn since the change that began this screen: an icon menu's
   layer may never be wiped either, and its selected label would stay the
   focus for good; and a settings row redrawn by turns of VALUE that changed
   nothing, before the tab changed, is the tab before's. */
static int want_lit(const struct item *it)
{
    return is_lit(it) && (int32_t)(it->last_drawn - b_prev) > 0
        && (int32_t)(it->last_drawn - screen_epoch) >= 0;
}
static int want_popup(const struct item *it) { return is_popup(it); }
static int want_new_popup(const struct item *it)
{
    return is_popup(it) && (it->fresh || it->changed == 1);
}
static int want_changed(const struct item *it) { return it->changed != 0; }
static int drawn_now(const struct item *it);
static int want_notice(const struct item *it)
{
    return it->role == ROLE_NOTICE && drawn_now(it);
}
/* An operation's message, Working, Operation Completed!, just drawn in the
   message box: a dialog's question there has its buttons with it, and is
   read with them as the dialog. */
static int want_message(const struct item *it)
{
    int i;

    if (it->role != ROLE_MESSAGE || !(it->fresh || it->changed == 1))
        return 0;
    for (i = 0; i < ITEMS; i++)
        if (live(&items[i]) && items[i].surf == it->surf && is_lit(&items[i]))
            return 0;
    return 1;
}
static int want_new_state(const struct item *it)
{
    return it->role == ROLE_STATE && (it->changed == 1 || (page_arrived && drawn_now(it)));
}

/* A tab strip drawn for this screen, which names the current tab itself. */
static int tab_known(void)
{
    return tab_now[0] != 0 && (int32_t)(tab_tick - screen_epoch) >= 0;
}

/* What a screen shows; a page count is left to the tab it counts. The
   pad operations page asks for pads in its status bar. Not the knobs'
   columns, an effect's or the pad and pattern settings', which are heard
   as they are turned, nor a grid page's effects when none is selected:
   its heading and page say where you are. */
static int knob_column(const struct item *it)
{
    return it->role == ROLE_EFFECT || it->role == ROLE_FXLABEL || it->role == ROLE_KNOB
        || it->role == ROLE_QUIET;
}

static int want_fresh(const struct item *it)
{
    return (!is_status(it) || it->role == ROLE_PICKS) && !is_title(it) && it->role != ROLE_TABS
        && it->role != ROLE_HINT && !knob_column(it) && it->role != ROLE_CELL
        && it->role != ROLE_COPY_FROM && it->role != ROLE_COPY_TO
        && (it->fresh || it->changed == 1) && !(tab_known() && page_number(it));
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
        /* A knob's value is not read arriving, and nor is its unit. */
        for (j = 0; j < ITEMS && !skip; j++)
            skip = live(&items[j]) && knob_column(&items[j]) && unit_of(&items[j]) == it;
        if (skip || (l != NULL && unit_of(l) == it))
            continue;
        if (it->role == ROLE_ANSWER && say_pair(it)) {
            ;
        } else if (l != NULL) {
            last_label = l;
            say3(l->text, full_text(it), unit_said(it, l));
        } else if (padlike(it->text)) {
            say_pad(it);
        } else {
            say(it);
        }
    }
}

static int layer_is_new(uint32_t surf);

static int drawn_now(const struct item *it)
{
    return (int32_t)(it->last_drawn - b_prev) > 0;
}

/* A page count, "2/ 5", drawn at the top: which of a tabbed page's tabs is
   showing. */
static int page_number(const struct item *it)
{
    const char *t = it->text;
    int n = 0;

    while (*t == ' ')
        t++;
    if (*t < '0' || *t > '9' || it->y > 5)
        return 0;
    while (*t >= '0' && *t <= '9')
        n = n * 10 + (*t++ - '0');
    return *t == '/' ? n : 0;
}

/* The name of the tab a tabbed page is on: its tabs in order along the top,
   and the page count saying which. */
static const struct item *current_tab(void)
{
    const struct item *tabs[16];
    int ntabs = 0, page = 0, i, j;

    for (i = 0; i < ITEMS; i++) {
        const struct item *it = &items[i];
        if (!live(it))
            continue;
        if (it->role == ROLE_TABS && ntabs < 16) {
            for (j = ntabs; j > 0 && tabs[j - 1]->x > it->x; j--)
                tabs[j] = tabs[j - 1];
            tabs[j] = it;
            ntabs++;
        } else if (page == 0 && drawn_now(it)) {
            page = page_number(it);
        }
    }
    return page >= 1 && page <= ntabs ? tabs[page - 1] : NULL;
}

/* Everything on a surface drawn since the last batch: a dialog shown again
   on the layer it used before finds its buttons there from the last time,
   drawn again now. */
static int layer_redrawn(uint32_t surf)
{
    int i;

    for (i = 0; i < ITEMS; i++)
        if (live(&items[i]) && items[i].surf == surf && !drawn_now(&items[i]))
            return 0;
    return 1;
}

/* A dialog that has just opened: a surface newly in use carrying pop-up
   text, not a title, and a focused button, as "Format SD Card, Are you
   sure?" with CANCEL selected. A page titled on a pop-up layer, as PAD LINK
   GROUPS, is not one. */
static uint32_t new_dialog(void)
{
    int i, k;

    for (i = 0; i < ITEMS; i++) {
        const struct item *it = &items[i];
        if (!live(it) || it->mark != -1 || (it->role != ROLE_NONE && it->role != ROLE_MESSAGE)
            || !it->fresh
            || !layer_redrawn(it->surf))
            continue;
        for (k = 0; k < ITEMS; k++)
            if (live(&items[k]) && items[k].surf == it->surf && is_lit(&items[k]))
                return it->surf;
    }
    return 0;
}

static uint32_t dialog_surf;
static int want_dialog_text(const struct item *it)
{
    return it->surf == dialog_surf && it->mark == -1;
}
static int want_dialog_focus(const struct item *it)
{
    return it->surf == dialog_surf && is_lit(it);
}

static char last_tab[ITEM_TEXT];

/* Screens with no title of their own, named as the buttons that open them
   are: PITCH/SPEED, SHIFT and START/END for CHOP, and START/END. */
static const struct { int page; const char *name; int alone; } page_names[] = {
    { 83, "PITCH/SPEED", 1 }, { 88, "CHOP", 1 }, { 90, "START/END", 1 },
    /* And two named as the manual does, which go on to say what they show,
       only when they are new: RECORD SETTING is built again as REMAIN
       changes its MODE. */
    { PAGE_RECORD_SETTING, "RECORD SETTING", 0 }, { PAGE_MICROSCOPE, "Microscope", 0 },
};

/* TR-REC, on the pattern screen, shown by its line Ptn:G1; and whether it
   was showing when the last batch was said. */
static int trrec_last;

static int trrec_showing(void)
{
    int i;

    for (i = 0; i < ITEMS; i++)
        if (live(&items[i]) && items[i].site == TRREC_PATTERN)
            return 1;
    return 0;
}

/* The REMAIN page, a line at a time, each name with its value. */
static int want_drawn(const struct item *it)
{
    return drawn_now(it) && !is_popup(it);
}

static void say_rows(struct item **order)
{
    int n = in_order(order, ITEMS, want_drawn), i, k;

    for (i = 0; i < n; i = k) {
        const char *part[3] = { NULL, NULL, NULL };
        int m = 0;
        for (k = i; k < n && order[k]->y - order[i]->y <= 2; k++)
            if (m < 3)
                part[m++] = order[k]->text;
        say3(part[0], part[1], part[2]);
    }
}

/* What to say about a new screen. A dialog that has just opened is all
   there is: its text, then its own focused button. Otherwise the title, the
   tab a tabbed page is on when it has changed, then the focus: the focused
   item, else a settings list's top row, else on the main screen the bank
   and pad; then any pop-up; and on a screen with none of those, what it
   shows. */
static void new_screen(struct item **order)
{
    const struct item *tab;
    int n, i, k, focus, popups, effect = 0, top = 0;

    last_label = NULL;
    last_fixed = 0;
    pair_said[0][0] = pair_said[1][0] = 0;
    if ((dialog_surf = new_dialog()) != 0) {
        n = in_order(order, ITEMS, want_dialog_text);
        for (i = 0; i < n; i++)
            say(order[i]);
        n = in_order(order, ITEMS, want_dialog_focus);
        for (i = 0; i < n; i++)
            say(order[i]);
        return;
    }
    /* REMAIN's page, shown while the key is held on the top screen, is
       all information. */
    if (page_now == PAGE_REMAIN) {
        say_rows(order);
        return;
    }
    n = in_order(order, ITEMS, want_title);
    for (i = 0; i < n; i++)
        if (!title_said(order[i]))
            say(order[i]);
    /* The pattern screen with no big title names itself by its state in
       the status bar, STOP-PTN C1, COUNT IN, COPY, EXCHANGE; TR-REC, whose
       status bar is empty, by its mode, as Roland names it. */
    if (n == 0 && page_now == PAGE_PATTERN) {
        if (trrec_showing()) {
            if (!trrec_last)
                n += say3("TR-REC", NULL, NULL) == 1;
        } else {
            k = in_order(order, ITEMS, want_new_state);
            for (i = 0; i < k; i++)
                n += say(order[i]) == 1;
        }
    }
    /* A screen's warning, after its title. */
    k = in_order(order, ITEMS, want_notice);
    for (i = 0; i < k; i++)
        say(order[i]);
    /* A screen named by its button says its name and nothing more, as the
       top screen does: START/END's pad and PITCH/SPEED's tempo line were
       more than he wanted arriving. */
    if (n == 0 && page_arrived)
        for (i = 0; i < (int)(sizeof page_names / sizeof page_names[0]); i++)
            if (page_names[i].page == page_now) {
                if (page_names[i].alone) {
                    say3(page_names[i].name, NULL, NULL);
                    return;
                }
                if (batch_page != page_now)
                    say3(page_names[i].name, NULL, NULL);
            }
    if (tab_known()) {
        if (strcmp(tab_now, last_tab) != 0) {
            memcpy(last_tab, tab_now, ITEM_TEXT);
            say3(tab_now, NULL, NULL);
        }
    } else if ((tab = current_tab()) != NULL && strcmp(tab->text, last_tab) != 0) {
        memcpy(last_tab, tab->text, ITEM_TEXT);
        say(tab);
    }
    /* The focus counts when it is there, even when the title has already
       said its words, as a grid's title names the focused cell. */
    focus = in_order(order, ITEMS, want_lit);
    for (i = 0; i < focus; i++)
        say_focus(order[i]);
    if (copy_bank((uint32_t)page_now))
        say3(copy_side((uint32_t)page_now) ? "destination" : "source", NULL, NULL);
    /* The main screen, the manual's top screen, is named so on arrival,
       known by its bank and pad just drawn: they stay on its layer under the
       screens opened over it. The effect display is drawn over it, and it
       redraws them in the same frames: arriving there reads the effect, and
       coming back from it, as it comes and goes with every touch of an
       effect's knob, says nothing. Another screen's pad field in its status
       bar names the pad. */
    for (i = 0; i < ITEMS && !effect; i++)
        effect = live(&items[i]) && items[i].role == ROLE_EFFECT;
    if (focus == 0 && !effect)
        for (i = 0; i < ITEMS; i++)
            if (live(&items[i]) && is_pad_field(&items[i]) && drawn_now(&items[i])) {
                if (items[i].role != ROLE_PAD) {
                    focus += say_pad(&items[i]) == 1;
                    continue;
                }
                /* Nor does letting go of REMAIN, whose page shows only
                   while it is held. */
                top = !effect_shown && !(page_arrived && page_prev == PAGE_REMAIN);
                focus++;
                break;
            }
    /* An operation's message comes before where it leaves you: "Operation
       Completed!, top screen". */
    popups = in_order(order, ITEMS, want_new_popup);
    for (i = 0; i < popups; i++)
        say(order[i]);
    if (top)
        say3("top screen", NULL, NULL);
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
        if (f != it && live(f) && is_lit(f) && clean(b, sizeof b, full_text(f)) > 0
            && strcmp(a, b) == 0)
            return 1;
    }
    return 0;
}

/* The pads pressed where a screen asks for pads, each with what the page
   made of it: a pad or a project chosen or let go, or the pattern now
   chosen. A press that changed nothing, one the page refused, says
   nothing. */
static void say_picks(void)
{
    char t[32];
    int i;

    int kept = 0;
    uint32_t seq;

    for (i = 0; i < npicks; i++) {
        int kind = picks[i].kind, index = picks[i].index;
        int32_t now = pick_state(kind, index), before = picks[i].before;

        /* A TR-REC step, once the sequencer has taken it, for the same
           sample on the same bar, still showing; "step 5 on". */
        if (kind == PICK_STEP) {
            if (!picks[i].resolved) {
                picks[kept++] = picks[i];
                continue;
            }
            log_line("%lu step pad %d note %d %s, was %ld now %ld, bar %04x\n",
                     (unsigned long)b_now, index & 0xFF, index >> 8,
                     picks[i].resolved == 2 ? "unchanged" : "changed", (long)before, (long)now,
                     step_mask(index >> 8));
            if (picks[i].resolved == 2 || now < 0 || before < 0 || now == before
                || page_now != PAGE_PATTERN || !trrec_showing() || !seq_state(&seq)
                || SEQ_WORD(seq, 0x5A) != index >> 8)
                continue;
            snprintf(t, sizeof t, "step %d %s", (index & 0xFF) + 1, now > 0 ? "on" : "off");
            say3(t, NULL, NULL);
            continue;
        }
        if (kind == PICK_PATTERN) {
            if (now == before || now < 0 || now >= 160)
                continue;
            snprintf(t, sizeof t, "pattern %c %d", 'A' + (int)(now / 16), (int)(now % 16) + 1);
        } else if ((now > 0) == (before > 0)) {
            continue;
        } else if (kind == PICK_PROJECT) {
            snprintf(t, sizeof t, "project %02d %s", index + 1, now > 0 ? "selected" : "deselected");
        } else {
            snprintf(t, sizeof t, "%c %d %s", 'A' + index / 16, index % 16 + 1,
                     now > 0 ? "selected" : "deselected");
        }
        say3(t, NULL, NULL);
    }
    npicks = kept;
}

/* The playback buttons pressed, each as its name and what it now is, in the
   words of the firmware's own BANK A GATE ON, for the pad it set; a press
   that set nothing says nothing. SHIFT and LOOP's forwards and backwards
   loop is the ping pong loop. */
static void say_presses(void)
{
    static const char *const names[] = { "BPM SYNC", "GATE", "LOOP", "REVERSE" };
    static const int32_t bits[] = { SET_SYNC, SET_GATE, SET_LOOP, SET_REVERSE };
    char t[32];
    int i;

    for (i = 0; i < npresses; i++) {
        int k = presses[i].key - KEY_BPM_SYNC;
        int32_t now = pad_settings(presses[i].index), before = presses[i].before;
        int32_t mask = k == 2 ? SET_LOOP | SET_PINGPONG : bits[k];

        if ((now & mask) == (before & mask))
            continue;
        if (k == 2 && (now & SET_LOOP) && (now & SET_PINGPONG))
            snprintf(t, sizeof t, "ping pong loop ON");
        else
            snprintf(t, sizeof t, "%s %s", names[k], now & bits[k] ? "ON" : "OFF");
        say3(t, NULL, NULL);
    }
    npresses = 0;
    /* EXT SOURCE, by the input's state and not the pad field, which shows
       EXT only while the button is held. */
    if (ext_pending && ext_before >= 0 && (EXT_ON != 0) != ext_before)
        say3(EXT_ON ? "EXT SOURCE ON" : "EXT SOURCE OFF", NULL, NULL);
    ext_pending = 0;
    ext_before = -1;
    if (side_pending && side_before > -2
        && (copy_side((uint32_t)page_now) != 0) != (side_before != 0))
        say3(copy_side((uint32_t)page_now) ? "destination" : "source", NULL, NULL);
    side_pending = 0;
    side_before = -2;
}

/* What to say about a change on the same screen: a message, whenever it is
   drawn; a pop-up that has just appeared, whole; the newly focused item;
   values that changed. On the main screen a pad hit says nothing, and nor
   does anything else it changes there, the status fields or the big tempo
   when the pads' tempos differ, but a bank change says the bank and pad; elsewhere a pad is named whenever it changes. Text that has
   only appeared, with nothing changed, stays quiet: a screen drawing the
   rest of itself, a meter's scale. */
static void same_screen(struct item **order)
{
    static struct item *pop[ITEMS];
    int n, i, k, m, pad_hit = 0, answers = 0;
    uint32_t popup_surf = 0, main_surf = 0;

    /* A choice newly made says what the screen now shows for it, changed or
       not: the BPM screen's tempo, when PROJECT gives way to the bank. */
    for (i = 0; i < ITEMS; i++)
        if (live(&items[i]) && items[i].framed && items[i].changed)
            break;
    if (i < ITEMS)
        for (k = 0; k < ITEMS; k++)
            if (live(&items[k]) && is_lit(&items[k]) && drawn_now(&items[k])
                && !items[k].changed)
                items[k].changed = 2;
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
    for (i = 0; i < n; i++) {
        struct item *it = order[i];
        const struct item *l;
        if (!it->changed || !is_lit(it))
            continue;
        if (it->by_pad) {
            it->changed = 0;
            continue;
        }
        /* The chain's highlight moving from slot to slot as it plays. */
        if (page_now == PAGE_CHAIN && it->changed == 2 && !it->prompted) {
            it->changed = 0;
            continue;
        }
        /* A value turned on the selected row says the value alone. */
        if (it->changed == 1 && (l = row_label(it)) != NULL && l->row_sel) {
            if (say3(it->text, NULL, NULL))
                it->changed = 0;
        } else if (say_focus(it)) {
            it->changed = 0;
        }
    }
    for (i = 0; i < n; i++)
        if (order[i]->changed == 1 && order[i]->fresh && order[i]->prompted
            && order[i]->role != ROLE_HINT && !is_lit(order[i]) && !is_title(order[i]))
            answers++;
    for (i = 0; i < n; i++)
        if (order[i]->changed == 1 && is_pad_field(order[i])) {
            pad_hit = 1;
            if (order[i]->role == ROLE_PAD)
                main_surf = order[i]->surf;
        }
    for (i = 0; i < n; i++) {
        struct item *it = order[i];
        if (it->changed != 1 || is_lit(it) || is_title(it))
            continue;
        /* A chain's slot filled with the pattern the highlighted one holds
           is news. */
        if (it->role != ROLE_ANSWER && mirrors_focus(it)) {
            it->changed = 0;
            continue;
        }
        /* The count of pads chosen, redrawn up to a frame after the press
           the pad itself has been said for. */
        if (it->role == ROLE_PICKS && pick_seen && it->last_change - last_pick <= 375u) {
            it->changed = 0;
            continue;
        }
        /* Before the wait for a value to hold still: pads played quickly
           would otherwise have the tempo said once they stopped. */
        if (main_surf != 0 && it->surf == main_surf && !is_pad_field(it) && !is_status(it)
            && !is_popup(it)) {
            it->changed = 0;
            continue;
        }
        /* The pattern's state, whatever changed it. */
        if (it->role == ROLE_STATE) {
            say(it);
            it->changed = 0;
            continue;
        }
        /* A bank COPY BANK PAD copies from or to, by the manual's words. */
        if (it->role == ROLE_COPY_FROM || it->role == ROLE_COPY_TO) {
            say3(it->role == ROLE_COPY_FROM ? "source" : "destination", it->text, NULL);
            it->changed = 0;
            continue;
        }
        /* And the pads a copy or an exchange is between. */
        if (it->role == ROLE_ANSWER && say_pair(it)) {
            it->changed = 0;
            continue;
        }
        if (it->by_pad) {
            it->changed = 0;
            continue;
        }
        if (it->rapid > 0 && b_now - it->last_change < STEADY)
            continue;
        /* A key's legend is heard when what you just did changed it, as
           turning VALUE changes the zoom, not as a screen sets it arriving. */
        if (it->role == ROLE_HINT) {
            if (!it->fresh && it->prompted && (int32_t)(it->last_change - screen_epoch) > 75)
                say(it);
            it->changed = 0;
            continue;
        }
        if (is_pad_field(it)) {
            /* The main screen's pad field says nothing: its bank is said as
               every screen's is, by its letter, and the EXT it shows while
               EXT SOURCE is held by the input's own state. */
            if (it->prev0 != it->text[0] && it->role != ROLE_PAD)
                say_pad(it);
        } else if (padlike(it->text) && is_status(it)) {
            say_pad(it);
        } else if (it->fresh) {
            /* Text that appears in answer to a key or a knob is said, as
               PLEASE SELECT SMPL is once SAMPLE is chosen; text a screen
               draws a moment after arriving, a meter's scale, is not, and
               nor is a whole display drawn back, as the effect display is
               when an effect's knob is touched again. */
            if (it->prompted && answers <= 2)
                say_value(it);
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
   anything is now showing three strings or more; or three strings already
   showing change their text at once; or after a wipe of half
   the screen or more, at least three strings are new and they make up half
   of what is showing. A pad hit wipes and redraws the status line, which
   changes but is not new; a screen drawing the rest of itself a moment
   later, with no wipe, is not a new screen. */
/* For the settings menu's phrases. */
int screen_say_text(const char *text)
{
    return say3(text, NULL, NULL);
}

/* What is showing, when nothing has just been drawn to say so: after the
   settings menu closes over a screen that did not change. The focus drawn
   most recently, with anything lit within a second of it; the newest title
   if it is no older than that; else the main screen's bank and pad. */
static void reannounce(void)
{
    const struct item *title = NULL, *pad = NULL;
    uint32_t newest = 0;
    int i, any = 0;

    for (i = 0; i < ITEMS; i++) {
        const struct item *it = &items[i];
        if (!live(it))
            continue;
        if (is_lit(it) && (!any || (int32_t)(it->last_drawn - newest) > 0)) {
            newest = it->last_drawn;
            any = 1;
        }
        if (is_title(it) && (title == NULL || (int32_t)(it->last_drawn - title->last_drawn) > 0))
            title = it;
        if (is_pad_field(it))
            pad = it;
    }
    if (title != NULL && (!any || (int32_t)(title->last_drawn + 750u - newest) >= 0))
        say(title);
    for (i = 0; any && i < ITEMS; i++)
        if (live(&items[i]) && is_lit(&items[i])
            && (int32_t)(items[i].last_drawn + 750u - newest) >= 0)
            say_focus(&items[i]);
    if (!any && pad != NULL && pad->role == ROLE_PAD)
        say3("top screen", NULL, NULL);
    else if (!any && pad != NULL)
        say_pad(pad);
}

static int screen_changed(void)
{
    uint32_t surf[16];
    int nsurf = 0, i, k, alive = 0, fresh = 0, replaced = 0;

    if (page_pending || tab_changed)
        return 1;

    for (i = 0; i < ITEMS; i++) {
        const struct item *it = &items[i];
        if (!live(it) || is_popup(it))
            continue;
        if (is_title(it) && it->changed == 1)
            return 1;
        alive++;
        if (it->fresh || (it->changed == 1 && !is_status(it)))
            fresh++;
        if (!it->fresh && it->changed == 1 && !is_status(it))
            replaced++;
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
    /* A list whose items all change at once, as the SD card menu's do on
       coming back from its submenu. */
    if (replaced >= 3)
        return 1;
    return wiped && fresh >= 3 && fresh * 2 >= alive;
}

/* The file lists play the sample the cursor lands on, so a quick scroll
   through samples is for hearing them, not their names: on a sample, the
   name waits until the cursor has rested two seconds, and what was being
   said stops as it lands. A folder is said as soon as the list settles. */
#define SAMPLE_REST MS_TO_TICKS(2000)

/* The file list's focused row, if it is a sample; the latest drawn, as the
   old row and the new are both lit for a moment while the focus moves. */
static const struct item *sample_focus(void)
{
    const struct item *f = NULL;
    int i;

    for (i = 0; i < ITEMS; i++) {
        const struct item *it = &items[i];
        if (live(it) && it->site == SCROLL_LIST && is_lit(it)
            && (f == NULL || (int32_t)(it->drawn_seq - f->drawn_seq) > 0))
            f = it;
    }
    return f != NULL && f->sample ? f : NULL;
}

/* Drains what the hooks caught. Once there has been no change for the
   settle time, or changes have kept coming for a second, or a value that was
   changing quickly has held still, fills phrases with what to say and
   answers 1. Answers 1 with no phrases when what is being said should
   stop. */
int screen_poll(char *phrases, size_t cap, int *count)
{
    static struct item *order[ITEMS];
    static const struct item *rest_on;
    static char rest_text[ITEM_TEXT];
    static uint32_t rest_since, stopped_at = (uint32_t)-1;
    const struct item *sample;
    uint32_t now = device_ticks();
    int i, due = 0, kind = 0, working = 0;

    while (draw_rd != draw_wr) {
        const struct draw *dr = &draws[draw_rd % DRAWS];
        if (take(dr)) {
            /* When the change was drawn, not when it was seen: the screen
               it starts begins there, and its own labels are no older. */
            if (!pending)
                first_pending = dr->tick;
            pending = 1;
            last_change = now;
        }
        __asm__ volatile("dmb" ::: "memory");
        draw_rd++;
    }
    /* The bank changes with no drawing on screens that do not show it, as
       the copy and delete screens do not. A pad on the pattern screen
       choosing a pattern to record moves it to the pattern's bank, which the
       recording screen that follows names with the pattern. */
    {
        int32_t bank = BANK_NOW;
        if (bank >= 0 && bank <= 9 && bank != bank_seen) {
            if (bank_seen >= 0
                && !(pad_page == PAGE_PATTERN && played_just_before(last_pad, pad_seen, now)
                     && !played_just_before(last_bank_key, bank_key_seen, now))) {
                if (!pending)
                    first_pending = now;
                pending = 1;
                last_change = now;
                bank_moved = 1;
            }
            bank_seen = bank;
        }
    }
    /* A TR-REC step changes a moment after its pad, as the sequencer takes
       the note: watched until it does, or a quarter of a second passes. */
    for (i = 0; i < npicks; i++)
        if (picks[i].kind == PICK_STEP && !picks[i].resolved) {
            if (pick_state(PICK_STEP, picks[i].index) != picks[i].before) {
                picks[i].resolved = 1;
            } else if (now - picks[i].tick > STEP_WAIT) {
                picks[i].resolved = 2;
            } else {
                continue;
            }
            if (!pending)
                first_pending = now;
            pending = 1;
            last_change = now;
        }
    /* The settings menu answers at once, and nothing else speaks while it
       is open, or while screen reading is off. */
    if (menu_n > 0) {
        int closed = 0;
        b_out = phrases;
        b_cap = cap;
        b_now = now;
        for (i = 0; i < menu_n; i++) {
            b_used = 0;
            b_count = 0;
            closed = menu_action(menu_q[i].action, menu_q[i].step);
        }
        menu_n = 0;
        if (closed && !muted)
            reannounce();
        for (i = 0; i < ITEMS; i++)
            items[i].changed = items[i].fresh = 0;
        pending = 0;
        tab_changed = 0;
        b_prev = now;
        *count = b_count;
#ifdef SIM
        sim_batch(phrases, b_count);
#endif
        log_line("%lu say menu %d\n", (unsigned long)now, b_count);
        return b_count > 0;
    }
    if (menu_is_open() || muted) {
        for (i = 0; i < ITEMS; i++)
            items[i].changed = items[i].fresh = 0;
        npicks = 0;
        npresses = 0;
        ext_pending = 0;
        ext_before = -1;
        side_pending = 0;
        side_before = -2;
        bank_moved = 0;
        pat_pending = 0;
        chain_removed[0] = 0;
        pending = 0;
        page_pending = 0;
        tab_changed = 0;
        return 0;
    }
    sample = sample_focus();
    if (sample != rest_on || (sample != NULL && strcmp(sample->text, rest_text) != 0)) {
        rest_on = sample;
        rest_since = now;
        if (sample != NULL) {
            memcpy(rest_text, sample->text, sizeof rest_text);
            /* Once for each batch said: moving on from sample to sample has
               nothing more to stop. */
            if (stopped_at != batches) {
                stopped_at = batches;
                *count = 0;
#ifdef SIM
                sim_stop();
#endif
                log_line("%lu stop\n", (unsigned long)now);
                return 1;
            }
        }
    }
    if (pending && (sample != NULL ? now - rest_since >= SAMPLE_REST
                    : now - last_change >= settle_ticks || now - first_pending >= 750u))
        due = 1;
    if (due && state_seen && (int32_t)(now - state_hold) < 0)
        due = 0;
    /* Only what can be said: a running bar and beat, never said, would
       otherwise make every poll a batch once it held still. */
    for (i = 0; i < ITEMS && !due; i++)
        if (live(&items[i]) && items[i].changed == 1 && items[i].rapid > 0
            && now - items[i].last_change >= STEADY)
            due = 1;
    if (!due)
        return 0;
    /* What was erased and not drawn back within a frame is gone, and so is a
       pop-up not drawn for three seconds: a pop-up's layer is not cleared
       when it closes, so its text would otherwise stay on the model for
       good. */
    for (i = 0; i < ITEMS; i++) {
        struct item *it = &items[i];
        if (!it->used)
            continue;
        if ((it->erased && now - it->erased > ERASED_LIFE)
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
    for (i = 0; i < ITEMS && !(live(&items[i]) && items[i].site == DEST_LINE); i++)
        ;
    if (i == ITEMS)
        dest_said[0] = 0;
    /* An operation's message comes first, before the bank letter and the
       screen it leaves you on: "Operation Completed!, pattern". While it
       says Working, the screen behind it is changing and is not said: the
       SELECT PROJECT screen redraws its title for the project it loads. */
    if (screen_mode != SCREEN_ALL) {
        int k = in_order(order, ITEMS, want_message);
        for (i = 0; i < k; i++) {
            say(order[i]);
            order[i]->changed = 0;
        }
        for (i = 0; i < ITEMS && !working; i++)
            working = live(&items[i]) && items[i].role == ROLE_MESSAGE
                && reads(items[i].text, "Working") && now - items[i].last_drawn < POPUP_LIFE
                && (!page_pending || (int32_t)(items[i].drawn_seq - page_seq) > 0);
    }
    /* A bank change is its letter alone, on every screen but the tempo
       screen, whose choice of PROJECT or BANK A reads it already. */
    if (working) {
        ;
    } else if (bank_moved && page_now != 81 && page_now != PAGE_PROJECT) {
        char t[2] = { (char)('A' + bank_seen), 0 };
        say3(t, NULL, NULL);
    } else if (pat_pending && page_now != 81) {
        /* In pattern mode the bank keys set the pattern store's banks, which
           nothing on the screen shows: the first of them a key moved. */
        int32_t banks = pat_banks();
        for (i = 0; i < PAT_BANKS; i++) {
            int b = (int)(int8_t)(banks >> (8 * i)), was = (int)(int8_t)(pat_before >> (8 * i));
            if (b != was && b >= 0 && b <= 9) {
                char t[2] = { (char)('A' + b), 0 };
                say3(t, NULL, NULL);
                break;
            }
        }
    }
    bank_moved = 0;
    pat_pending = 0;
    say_picks();
    say_presses();
    if (chain_removed[0] && page_now == PAGE_CHAIN)
        say3("delete,", chain_removed, NULL);
    chain_removed[0] = 0;
    if (working) {
        for (i = 0; i < ITEMS; i++)
            if (items[i].role != ROLE_MESSAGE)
                items[i].changed = 0;
    } else if (screen_mode == SCREEN_ALL) {
        for (i = 0; i < nburst; i++)
            say(&items[burst[i]]);
        nburst = 0;
        for (i = 0; i < ITEMS; i++)
            items[i].changed = 0;
    } else if ((kind = screen_changed()) != 0) {
        screen_epoch = first_pending - 2u;
        /* A page just built has drawn all it shows; what it has not drawn
           belongs to the page before, left on a layer never wiped, and
           drawn a moment ago if that page redrew itself continually, as
           the BPM screen does. */
        if (page_pending)
            for (i = 0; i < ITEMS; i++) {
                struct item *it = &items[i];
                if (!it->used || (int32_t)(it->drawn_seq - page_seq) > 0)
                    continue;
                if (text_len(it) > 0 && it->last_drawn - it->first_drawn >= 750u)
                    log_line("%lu gone %08lx %d %d |%s|\n", (unsigned long)now,
                             (unsigned long)it->surf, it->x, it->y, it->text);
                it->used = 0;
            }
        page_arrived = page_pending;
        page_pending = 0;
        new_screen(order);
        /* A screen that appears because a pad was played says nothing: the
           first the reader sees after starting is the main screen redrawn
           for a pad hit, and a pad on PITCH/SPEED redraws its strip. Nor
           does the effects grid an effect's button shows while it is held,
           whether it is turning the effect on or off: the effect's page,
           when it comes, names it. */
        if (!page_arrived && pads_play() && pad_seen && first_pending - last_pad <= PLAY_WINDOW) {
            b_used = 0;
            b_count = 0;
        }
        for (i = 0; i < ITEMS && fx_seen && first_pending - last_fx <= TURN_WINDOW; i++)
            if (live(&items[i]) && items[i].role == ROLE_CELL) {
                b_used = 0;
                b_count = 0;
                break;
            }
        for (i = 0; i < ITEMS; i++)
            items[i].changed = items[i].own = 0;
    } else {
        same_screen(order);
        for (i = 0; i < ITEMS; i++)
            if (items[i].changed == 2 || !live(&items[i]))
                items[i].changed = 0;
    }
    effect_shown = 0;
    for (i = 0; i < ITEMS && !effect_shown; i++)
        effect_shown = live(&items[i])
            && (items[i].role == ROLE_EFFECT || items[i].role == ROLE_CELL);
    for (i = 0; i < ITEMS; i++)
        items[i].fresh = 0;
    wiped = 0;
    tab_changed = 0;
    b_prev = now;
    batch_page = page_now;
    trrec_last = trrec_showing();
    /* A screen that says just what the one before said, less than a second
       ago with no key pressed between: an effect's button shows the grid,
       titled with the effect, for a tenth of a second before the effect's
       page, which names it. Pressed again, it is said again. */
    {
        static char last[256];
        static size_t last_len;
        static uint32_t last_tick;
        if (b_count > 0 && b_used <= sizeof last) {
            if (kind && b_used == last_len && memcmp(last, phrases, b_used) == 0
                && now - last_tick < 750u
                && !(press_seen && (int32_t)(last_press - last_tick) > 0)) {
                b_count = 0;
            } else {
                memcpy(last, phrases, b_used);
                last_len = b_used;
            }
            last_tick = now;
        }
    }
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
    printf("screen: %lu draws seen, %lu lost, %lu batches, %lu items evicted, %lu log lines dropped, "
           "%lu page messages counted\n",
           (unsigned long)draw_seen, (unsigned long)draw_lost, (unsigned long)batches,
           (unsigned long)evicted, (unsigned long)log_dropped, (unsigned long)page_repeats);
}

/* What the draw log has gained since the last time, onto its part. Past 48
   KB the part ends with every item on record and how often it was drawn,
   and the next begins. */
#define LOG_PART (48u * 1024u)

void screen_log_note(const char *what)
{
    log_line("# %s, at %lu ticks\n", what, (unsigned long)device_ticks());
}

uint32_t screen_log_dropped(void)
{
    return log_dropped;
}

/* The room past the log, for copying files through while no part is being
   written. */
void *screen_log_spare(size_t *len)
{
    *len = SUMMARY;
    return draw_log + LOG_CAP - SUMMARY;
}

int screen_log_write(int last)
{
    static size_t part_len;
    static int part;
    size_t n = log_len;
    int i, k, ends = last || part_len + log_len >= LOG_PART;

    if (log_len == 0 && !last)
        return 0;
    if (ends) {
        k = snprintf(draw_log + n, LOG_CAP - n,
                     "# items: surface x y draws colour task caller site erased |text|\n");
        if (k > 0)
            n += (size_t)k;
    }
    for (i = 0; ends && i < ITEMS && n + 128 < LOG_CAP; i++) {
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
    if (!log_append(LOG_DRAWS, part, draw_log, n))
        return 0;
    part_len += log_len;
    log_len = 0;
    if (ends) {
        part++;
        part_len = 0;
        log_line("# part %d, from %lu ticks, %lu lines dropped, %lu page messages counted so far\n",
                 part, (unsigned long)device_ticks(), (unsigned long)log_dropped,
                 (unsigned long)page_repeats);
    }
    return 1;
}
