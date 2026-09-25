/* Seeing what the engine does on the instrument, where nothing can watch.

   Stage markers rewrite A:/EVV/STAGE.TXT at each step, so a hang leaves the
   last step reached on the card.

   The fault catcher takes over the four fault vectors for the length of a
   run (VTOR is 0 and the table is in ITCM, which is writable). A fault from
   thread mode records the fault status registers and the stacked frame, then
   rewrites the stacked return address so that returning from the exception
   lands in fault_recover, still in thread mode, on the faulting context's
   stack. That writes A:/EVV/FAULT.TXT and leaves through engine_exit, so the
   firmware gets its task back instead of freezing. A fault in handler mode
   cannot be walked back like that; it records and stops. */

#include <stdint.h>
#include <stdio.h>
#include "device.h"

typedef int (*open_fn) (const char *path, int mode);
typedef int (*write_fn)(int h, const void *buf, int len);
typedef int (*close_fn)(int h);
#define F_OPEN  ((open_fn) 0x800DEC61u)
#define F_WRITE ((write_fn)0x800DD9B9u)
#define F_CLOSE ((close_fn)0x800DEBD9u)

/* From 0x0C, the HardFault slot: a table based at zero reads to the compiler
   as a null pointer, which it may turn into a trap. */
#define VEC   ((volatile uint32_t *)0x0000000Cu)
#define CFSR  (*(volatile uint32_t *)0xE000ED28u)
#define HFSR  (*(volatile uint32_t *)0xE000ED2Cu)
#define MMFAR (*(volatile uint32_t *)0xE000ED34u)
#define BFAR  (*(volatile uint32_t *)0xE000ED38u)

__attribute__((noreturn)) void engine_exit(int code);
uint32_t device_ticks(void);

struct fault {
    uint32_t vector, exc_return, cfsr, hfsr, mmfar, bfar;
    uint32_t r0, r1, r2, r3, r12, lr, pc, xpsr, sp;
    uint32_t stage;
};

volatile struct fault fault_record;
__attribute__((used)) uint32_t saved_vectors[4];
static volatile uint32_t stage_count;

static int hexout(char *p, uint32_t v)
{
    static const char hex[] = "0123456789ABCDEF";
    int i;

    for (i = 0; i < 8; i++)
        p[i] = hex[(v >> (28 - 4 * i)) & 15];
    return 8;
}

static void write_text(const char *path, const char *s, int n)
{
    int h = F_OPEN(path, 0x601);

    if (h >= 0) {
        F_WRITE(h, s, n);
        F_CLOSE(h);
    }
}

/* Each stage goes into the log with its time in audio interrupts (750 a
   second); nothing is written to the card mid-run, since a file write costs
   tens of milliseconds and would be timed along with the engine. A fault
   still reports the last stage reached, through FAULT.TXT. */
void target_stage(const char *what)
{
    stage_count++;
    printf("stage %2lu at %5lu ticks: %s\n", (unsigned long)stage_count,
           (unsigned long)device_ticks(), what);
}

static void fault_recover(void)
{
    static const char *names[] = {
        "vector", "exc_return", "CFSR", "HFSR", "MMFAR", "BFAR",
        "r0", "r1", "r2", "r3", "r12", "lr", "pc", "xpsr", "sp", "stage"
    };
    char text[512];
    const volatile uint32_t *w = (const volatile uint32_t *)&fault_record;
    int n = 0, i;

    for (i = 0; i < 16; i++) {
        const char *s = names[i];
        while (*s)
            text[n++] = *s++;
        text[n++] = ' ';
        n += hexout(text + n, w[i]);
        text[n++] = '\r';
        text[n++] = '\n';
    }
    if (card_log)
        write_text("A:/EVV/FAULT.TXT", text, n);
    engine_exit(99);
}

/* r0 is the stacked frame, r1 the EXC_RETURN value, r2 the vector number. */
__attribute__((used)) static uint32_t fault_c(uint32_t *frame, uint32_t exc_return, uint32_t vector)
{
    fault_record.vector = vector;
    fault_record.exc_return = exc_return;
    fault_record.cfsr = CFSR;
    fault_record.hfsr = HFSR;
    fault_record.mmfar = MMFAR;
    fault_record.bfar = BFAR;
    fault_record.r0 = frame[0];
    fault_record.r1 = frame[1];
    fault_record.r2 = frame[2];
    fault_record.r3 = frame[3];
    fault_record.r12 = frame[4];
    fault_record.lr = frame[5];
    fault_record.pc = frame[6];
    fault_record.xpsr = frame[7];
    fault_record.sp = (uint32_t)frame;
    fault_record.stage = stage_count;

    if ((exc_return & 0xFu) == 0x1u)          /* from handler mode: no way back */
        for (;;)
            ;
    CFSR = CFSR;                              /* write-one-to-clear */
    HFSR = HFSR;
    frame[6] = (uint32_t)fault_recover & ~1u;
    frame[7] = 0x01000000u;                   /* Thumb, no IT state */
    return exc_return;
}

/* Only faults in the engine's own code, in the engine's own task, are ours
   to recover. Anything else goes straight to the saved vector with every
   register as the fault left it. A fault in the engine's code on another
   task's time means the screen hook, running inside the interface; that
   cannot be walked back, but the hook is taken out first so that nothing
   calls into it again. */
extern char __image_start[], __image_end[];
int engine_task_id(void);
int kernel_task_self(void);
void screen_remove(void);

__attribute__((used)) static uint32_t fault_is_ours(uint32_t *frame)
{
    uint32_t pc = frame[6];

    if (pc < (uint32_t)__image_start || pc >= (uint32_t)__image_end)
        return 0;
    if (kernel_task_self() == engine_task_id())
        return 1;
    screen_remove();
    return 0;
}

#define FAULT_ENTRY(name, num)                                   \
    __attribute__((naked)) static void name(void)                \
    {                                                            \
        __asm__ volatile(                                        \
            "tst   lr, #4\n"                                     \
            "ite   eq\n"                                         \
            "mrseq r0, msp\n"                                    \
            "mrsne r0, psp\n"                                    \
            "push  {r0, lr}\n"                                   \
            "bl    fault_is_ours\n"                              \
            "mov   r3, r0\n"                                     \
            "pop   {r0, lr}\n"                                   \
            "cbnz  r3, 1f\n"                                     \
            "ldr   r3, =saved_vectors\n"                         \
            "ldr   r3, [r3, #(" #num " - 3) * 4]\n"              \
            "bx    r3\n"                                         \
            "1:\n"                                               \
            "mov   r1, lr\n"                                     \
            "movs  r2, #" #num "\n"                              \
            "push  {r1, lr}\n"                                   \
            "bl    fault_c\n"                                    \
            "pop   {r1, lr}\n"                                   \
            "bx    lr\n"                                         \
            ".ltorg\n");                                         \
    }

FAULT_ENTRY(hard_fault, 3)
FAULT_ENTRY(mem_fault, 4)
FAULT_ENTRY(bus_fault, 5)
FAULT_ENTRY(usage_fault, 6)

void diag_install(void)
{
    int i;

    for (i = 0; i < 4; i++)
        saved_vectors[i] = VEC[i];
    VEC[0] = (uint32_t)hard_fault | 1u;
    VEC[1] = (uint32_t)mem_fault | 1u;
    VEC[2] = (uint32_t)bus_fault | 1u;
    VEC[3] = (uint32_t)usage_fault | 1u;
    __asm__ volatile("dsb\n isb" ::: "memory");
}

void diag_remove(void)
{
    int i;

    for (i = 0; i < 4; i++)
        VEC[i] = saved_vectors[i];
    __asm__ volatile("dsb\n isb" ::: "memory");
}
