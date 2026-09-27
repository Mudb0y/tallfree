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
#define EXPORT_PAGE     (*(volatile uint32_t *)0x80CFD4E4u)
#define PADOPS_PAGE     (*(volatile uint32_t *)0x80CFD490u)
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
extern volatile uint32_t sim_icon_slots[2], sim_icon_ctx;
extern volatile uint32_t sim_page_table[94], sim_row_word, sim_tab_word;
extern volatile uint32_t sim_store[0x600 / 4], sim_export_page, sim_padops_page;
extern volatile int32_t sim_bank;
#define PICK_STORE      ((uint32_t)(uintptr_t)sim_store)
#define BANK_NOW        sim_bank
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
       ROLE_CHOICE, ROLE_EFFECT, ROLE_PICKS, ROLE_UNLIT };
static const struct { uint32_t site; uint8_t role; } sites[] = {
    { TITLE_SITE,  ROLE_TITLE },            /* the page title setter */
    { 0x80151801u, ROLE_TITLE },            /* EXPORT SAMPLE/PROJ./MULTIPAD */
    { 0x801120A1u, ROLE_TITLE },            /* CHROMATIC MODE */
    { 0x80146135u, ROLE_TITLE },            /* an effect's name, over its grid or page */
    { 0x80146545u, ROLE_EFFECT },           /* an effect's values, FUN_80146260, the
                                               effect page's alone */
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
    { 0x80151927u, ROLE_TITLE },            /* the export page's heading over PLEASE
                                               SELECT SMPL, FUN_801518B0 */
    { 0x8016BAB7u, ROLE_IGNORE },           /* the pattern settings' hint, SHIFT:OTHER */
    { 0x8016B061u, ROLE_UNLIT },            /* their quantise grid, GRID 16, on white */
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
    uint8_t  kind, task, len;
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

/* f holds r0 to r5, r12 and lr as the caller left them, then the caller's
   stack from the stacked length up. Slot 0x18 is the surface's background
   colour, the getter DrawString itself asks. */
__attribute__((used)) static void text_record(const uint32_t *f)
{
    struct draw d;
    const char *str = (const char *)f[3];
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
   the page and its mode, as FUN_80151688 (export) and FUN_8012E0B8 (pad
   operations) make it. The pad operations' other modes, copy and exchange,
   draw their pads as text. */
#define PAGE_PADOPS 67
#define PAGE_EXPORT 85
#define PAGE_MODE(p) (*(volatile int32_t *)((p) + 0x1A90u))
enum { PICK_NONE, PICK_SAMPLE, PICK_PROJECT, PICK_PATTERN, PICK_PADS };

static int pick_kind(uint32_t page, int pad, int *index)
{
    uint32_t p;
    int32_t bank = BANK_NOW;

    if (pad < 0 || pad > 15 || bank < 0 || bank > 9)
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
    }
    return 0;
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
    uint8_t  used, changed, rapid, task, title, fresh, role, icon_sel, row_sel, framed, own;
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
   page has made its choice, and when the last was pressed. */
static struct { int16_t kind, index; int32_t before; } picks[8];
static int npicks;
static uint32_t last_pick;
static uint8_t pick_seen;

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

/* When a knob, VALUE or CTRL, last moved, and a key was last pressed, as
   the pages were sent them. A page redraws what a knob changed within 40
   ticks of it, one frame, in every log so far. */
#define TURN_WINDOW 60u                      /* ticks, 80 ms */
static uint32_t last_turn, last_press, last_ctrl_logged;
static uint8_t turn_seen, press_seen;
static int16_t ctrl_logged = -1;

static int just_after(uint32_t then, int seen, uint32_t now)
{
    return seen && now - then <= TURN_WINDOW;
}

/* The draw log: every change of text or background in full; at the end a
   count of each item's redraws, so a screen that redraws continually costs
   a line per item.
   Text leaving the screen is logged when it is taken off the model, not at
   each clear: some screens clear and redraw twenty-five times a second. */
#define LOG_CAP  (128u * 1024u)
#define SUMMARY  (32u * 1024u)
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
static struct item *find(const struct draw *d)
{
    struct item *free_one = NULL, *oldest = NULL;
    int i;

    for (i = 0; i < ITEMS; i++) {
        struct item *it = &items[i];
        if (it->used && it->surf == d->surf && it->x == d->x && it->y == d->y)
            return it;
    }
    for (i = 0; i < ITEMS; i++) {
        struct item *it = &items[i];
        if (it->used && it->surf == d->surf && it->y == d->y && it->site == d->site
            && d->tick - it->last_drawn > 2u && overlaps(it, d)) {
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
   selected icon, a selected settings row, or a choice in its outline. */
static int is_lit(const struct item *it)
{
    return (it->mark == WHITE && it->y >= 10 && !it->title && it->role != ROLE_UNLIT)
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
    if (d->kind == EV_PAGE && d->x == 16) {
        log_line("%lu pick %d, pad %d, index %d, was %ld, page %ld\n", (unsigned long)d->tick,
                 d->y1, d->y, d->x1, (long)d->ink, (long)d->mark);
        if (npicks < (int)(sizeof picks / sizeof picks[0])) {
            picks[npicks].kind = d->y1;
            picks[npicks].index = d->x1;
            picks[npicks].before = d->ink;
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
        return 0;
    }
    if (d->kind == EV_PAGE) {
        log_line("%lu page %ld message %d\n", (unsigned long)d->tick, (long)d->mark, d->x);
        if (d->x != 1)
            return 0;
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
    it->title = it->role == ROLE_TITLE;
    text_changed = strcmp(it->text, d->text) != 0;
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

        /* Text that changes with nothing just pressed or turned changes on
           its own, as a meter, a clock or the sequencer's bar does, and is
           never taken for a value turned until the screen changes, however
           busy the knobs are meanwhile. */
        if (it->last_change != 0 && !turned && !just_after(last_press, press_seen, d->tick))
            it->own = 1;
        /* A first change is said at once. One that follows another within
           STEADY neither interrupts nor is said until the item has held still
           that long: a meter or a clock that never stops is not heard at all,
           and an effect being played is heard where it stops. A value just
           turned is said at every step, as the focus is. A pad, and the main
           screen's status, change only when something is done, however
           quickly, so they are dealt with at once. */
        if (turned && !it->own && it->role != ROLE_EFFECT) {
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
        if (live(o) && o != it && o->surf == it->surf && strlen(o->text) > n
            && strncmp(o->text, it->text, n) == 0 && o->text[n] != '.')
            return o;
    }
    return it;
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

/* Adds a phrase made of up to three texts. Answers 1 if added, 2 if the
   batch already says it, 0 if it says nothing or there is no room. */
static int say3(const char *a, const char *b, const char *c)
{
    char buf[3 * ITEM_TEXT], part[ITEM_TEXT];
    size_t n = 0;
    const char *src[3] = { a, b, c };
    int i;

    buf[0] = 0;
    for (i = 0; i < 3; i++) {
        size_t k;
        if (src[i] == NULL)
            continue;
        k = clean(part, sizeof part, src[i]);
        if (k == 0)
            continue;
        if (n > 0)
            buf[n++] = ' ';
        memcpy(buf + n, part, k + 1);
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

static int say(const struct item *it)
{
    return say3(full_name(it)->text, NULL, NULL);
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
    size_t n = strlen(it->text);

    if (n + 2 > sizeof t)
        return 0;
    if (it->text[1] == '-' || it->text[1] == ' ') {
        memcpy(t, it->text, n + 1);
    } else {
        t[0] = it->text[0];
        memcpy(t + 1, it->text, n + 1);
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
static int plain(const struct item *it)
{
    return !is_title(it) && !is_status(it) && it->role != ROLE_TABS && !it->icon_sel
        && !it->framed && !(it->mark == WHITE && it->y >= 10);
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
static const struct item *row_label(const struct item *v)
{
    const struct item *best = NULL;
    int i;

    for (i = 0; i < ITEMS; i++) {
        const struct item *o = &items[i];
        if (!live(o) || o == v || o->surf != v->surf || !plain(o) || o->site == v->site
            || o->mark == v->mark || o->x >= v->x || o->y - v->y > 2 || v->y - o->y > 2
            || (int32_t)(o->last_drawn - screen_epoch) < 0)
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
            || (heading && (!column_top(o) || !centred(o, v))))
            continue;
        if (best == NULL || o->y > best->y)
            best = o;
    }
    return best;
}

/* Above in its column, the nearest text within twenty pixels, else text
   heading its column within thirty-two and centred over it: a value two
   rows down names nothing below it, as the effect page's does not name the
   effect, and nor does a prompt name a hint under it, as PLEASE SELECT SMPL
   does not name ENTER:EX. */
static const struct item *label_of(const struct item *v)
{
    const struct item *best = row_label(v);

    if (best == NULL)
        best = column_label(v, 20, 0);
    if (best == NULL)
        best = column_label(v, 32, 1);
    return best;
}

/* A value's unit is short text just below it in its column, "Hz" under
   827; only a value with a label has one, and neither a value named on its
   own row, the next setting down, nor a pad or pattern, as the pattern
   settings show A-1 under GRID 16, is a unit. */
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
        if (clean(t, sizeof t, o->text) == 0 || strlen(t) > 4 || padlike(o->text)
            || row_label(o) != NULL)
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

    if (l != NULL && l != last_label) {
        last_label = l;
        return say3(l->text, v->text, unit_said(v, l));
    }
    return say3(v->text, NULL, NULL);
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
static int page_number(const struct item *it);

/* A tab strip drawn for this screen, which names the current tab itself. */
static int tab_known(void)
{
    return tab_now[0] != 0 && (int32_t)(tab_tick - screen_epoch) >= 0;
}

/* What a screen shows; a page count is left to the tab it counts. The
   pad operations page asks for pads in its status bar. */
static int want_fresh(const struct item *it)
{
    return (!is_status(it) || it->role == ROLE_PICKS) && !is_title(it) && it->role != ROLE_TABS
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
        if (skip || (l != NULL && unit_of(l) == it))
            continue;
        if (l != NULL) {
            last_label = l;
            say3(l->text, it->text, unit_said(it, l));
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

/* A dialog that has just opened: a surface newly in use carrying pop-up
   text, not a title, and a focused button, as "Format SD Card, Are you
   sure?" with CANCEL selected. A page titled on a pop-up layer, as PAD LINK
   GROUPS, is not one. */
static uint32_t new_dialog(void)
{
    int i, k;

    for (i = 0; i < ITEMS; i++) {
        const struct item *it = &items[i];
        if (!live(it) || it->mark != -1 || it->role != ROLE_NONE || !it->fresh
            || !layer_is_new(it->surf))
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

/* What to say about a new screen. A dialog that has just opened is all
   there is: its text, then its own focused button. Otherwise the title, the
   tab a tabbed page is on when it has changed, then the focus: the focused
   item, else a settings list's top row, else on the main screen the bank
   and pad; then any pop-up; and on a screen with none of those, what it
   shows. */
static void new_screen(struct item **order)
{
    const struct item *tab;
    int n, i, focus, popups;

    last_label = NULL;
    if ((dialog_surf = new_dialog()) != 0) {
        n = in_order(order, ITEMS, want_dialog_text);
        for (i = 0; i < n; i++)
            say(order[i]);
        n = in_order(order, ITEMS, want_dialog_focus);
        for (i = 0; i < n; i++)
            say(order[i]);
        return;
    }
    n = in_order(order, ITEMS, want_title);
    for (i = 0; i < n; i++)
        say(order[i]);
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
    /* Only a bank and pad just drawn: the main screen's stay on its layer
       under the screens opened over it. */
    if (focus == 0)
        for (i = 0; i < ITEMS; i++)
            if (live(&items[i]) && is_pad_field(&items[i]) && drawn_now(&items[i]))
                focus += say_pad(&items[i]) == 1;
    popups = in_order(order, ITEMS, want_new_popup);
    for (i = 0; i < popups; i++)
        say(order[i]);
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
        if (f != it && live(f) && is_lit(f) && clean(b, sizeof b, full_name(f)->text) > 0
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

    for (i = 0; i < npicks; i++) {
        int kind = picks[i].kind, index = picks[i].index;
        int32_t now = pick_state(kind, index), before = picks[i].before;

        if (kind == PICK_PATTERN) {
            if (now == before || now < 0 || now >= 160)
                continue;
            snprintf(t, sizeof t, "pattern %c %d", 'A' + (int)(now / 16), (int)(now % 16) + 1);
        } else if ((now > 0) == (before > 0)) {
            continue;
        } else if (kind == PICK_PROJECT) {
            snprintf(t, sizeof t, "project %d %s", index + 1, now > 0 ? "selected" : "deselected");
        } else {
            snprintf(t, sizeof t, "%c %d %s", 'A' + index / 16, index % 16 + 1,
                     now > 0 ? "selected" : "deselected");
        }
        say3(t, NULL, NULL);
    }
    npicks = 0;
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
    int n, i, k, m, pad_hit = 0;
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
        /* A value turned on the selected row says the value alone. */
        if (it->changed == 1 && (l = row_label(it)) != NULL && l->row_sel) {
            if (say3(it->text, NULL, NULL))
                it->changed = 0;
        } else if (say_focus(it)) {
            it->changed = 0;
        }
    }
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
        if (mirrors_focus(it)) {
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
        if (it->rapid > 0 && b_now - it->last_change < STEADY)
            continue;
        if (is_pad_field(it)) {
            if (it->prev0 != it->text[0])
                say_pad(it);
        } else if (padlike(it->text) && is_status(it)) {
            say_pad(it);
        } else if (it->fresh) {
            /* appeared, not changed */
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
    if (!any && pad != NULL)
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

/* Drains what the hooks caught. Once there has been no change for the
   settle time, or changes have kept coming for a second, or a value that was
   changing quickly has held still, fills phrases with what to say and
   answers 1. */
int screen_poll(char *phrases, size_t cap, int *count)
{
    static struct item *order[ITEMS];
    uint32_t now = device_ticks();
    int i, due = 0, kind = 0;

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
        pending = 0;
        page_pending = 0;
        tab_changed = 0;
        return 0;
    }
    if (pending && (now - last_change >= settle_ticks || now - first_pending >= 750u))
        due = 1;
    for (i = 0; i < ITEMS && !due; i++)
        if (items[i].used && items[i].changed == 1 && items[i].rapid > 0
            && now - items[i].last_change >= STEADY)
            due = 1;
    if (!due)
        return 0;
    /* What was erased and not drawn again is gone, and so is a pop-up not
       drawn for three seconds: a pop-up's layer is not cleared when it
       closes, so its text would otherwise stay on the model for good. */
    for (i = 0; i < ITEMS; i++) {
        struct item *it = &items[i];
        if (!it->used)
            continue;
        if ((it->erased && now - it->erased > 2u)
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
    say_picks();
    if (screen_mode == SCREEN_ALL) {
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
        page_pending = 0;
        new_screen(order);
        for (i = 0; i < ITEMS; i++)
            items[i].changed = items[i].own = 0;
    } else {
        same_screen(order);
        for (i = 0; i < ITEMS; i++)
            if (items[i].changed == 2 || !items[i].used)
                items[i].changed = 0;
    }
    for (i = 0; i < ITEMS; i++)
        items[i].fresh = 0;
    wiped = 0;
    tab_changed = 0;
    b_prev = now;
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
    k = snprintf(draw_log + n, LOG_CAP - n,
                 "# items: surface x y draws colour task caller site erased |text|\n");
    if (k > 0)
        n += (size_t)k;
    for (i = 0; i < ITEMS && n + 128 < LOG_CAP; i++) {
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
    write_file("A:/TALLFREE/DRAWS.TXT", draw_log, n);
    return 1;
}
