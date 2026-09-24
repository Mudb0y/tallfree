/* What src/port/evv_port.h asks for, on one core with no kernel of ours.

   The engine needs two contexts, not two processors: its synthesis thread
   blocks every time a buffer fills until the caller's thread takes it. So
   this is a cooperative scheduler. A context runs until it waits, and only a
   wait switches. With no preemption, the low lock has nothing to exclude.

   Time is virtual. When every context is waiting, the clock jumps to the
   earliest deadline rather than spinning towards it, which makes a run
   deterministic and as fast as the processor allows. The instrument will get
   a port over its own kernel's tasks; this one is for QEMU and for the first
   on-device render, where synthesis blocks the caller until it is done.

   Each context has its own thread-local block. The engine keeps its rule
   frame stack and landing places in __thread variables, and two contexts
   sharing them would interleave one context's frames with the other's. */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "evv_port.h"

void evv_frame_done(void);
void coop_fatal(const char *why);

#define MAX_TASKS 8

enum { READY, BLOCKED, DEAD, FREE };

struct evv_task {
    uint32_t     sp;
    int          state;
    int          timed;
    unsigned     deadline;
    const void  *waiting_on;
    void        *stack;
    void        *tls;
    void       (*entry)(void *);
    void        *arg;
};

struct evv_sem {
    int count;
    int most;
};

struct evv_event {
    int      signalled;
    unsigned pulses;
};

static struct evv_task tasks[MAX_TASKS];
static struct evv_task *current;
static unsigned now_ms;

/* Read by __aeabi_read_tp below: the thread pointer of the running context. */
void *coop_tp;

extern const char __tdata_start[], __tdata_end[];
extern const char __tbss_size[], __tls_align[];

/* ARM EABI TLS is variant 1: the thread pointer names an eight byte control
   block and the variables follow it at the block's alignment. */
static uint32_t tls_offset(void)
{
    uint32_t a = (uint32_t)__tls_align;
    if (a < 8)
        a = 8;
    return (8u + a - 1u) & ~(a - 1u);
}

static void *tls_new(void)
{
    uint32_t data = (uint32_t)(__tdata_end - __tdata_start);
    uint32_t bss = (uint32_t)__tbss_size;
    uint32_t off = tls_offset();
    char *b = malloc(off + data + bss + 8u);

    if (b == NULL)
        return NULL;
    memset(b, 0, off);
    memcpy(b + off, __tdata_start, data);
    memset(b + off + data, 0, bss);
    return b;
}

__asm__(
".syntax unified\n"
".thumb\n"
".text\n"
".globl __aeabi_read_tp\n"
".thumb_func\n"
".type __aeabi_read_tp, %function\n"
/* Called by compiled code for every __thread access; it may change r0 and
   nothing else. */
"__aeabi_read_tp:\n"
"    ldr r0, =coop_tp\n"
"    ldr r0, [r0]\n"
"    bx  lr\n"
".ltorg\n"

".globl coop_switch\n"
".thumb_func\n"
".type coop_switch, %function\n"
/* coop_switch(&from->sp, to->sp): the callee-saved registers go on the old
   stack, its pointer into *r0, and the new stack's are popped. A new context's
   stack is laid out so that the pop lands in coop_trampoline. */
"coop_switch:\n"
"    push  {r4-r11, lr}\n"
"    vpush {d8-d15}\n"
"    str   sp, [r0]\n"
"    mov   sp, r1\n"
"    vpop  {d8-d15}\n"
"    pop   {r4-r11, pc}\n"
);

void coop_switch(uint32_t *save_sp, uint32_t new_sp);

static void run_next(void);

static void coop_trampoline(void)
{
    struct evv_task *t = current;

    t->entry(t->arg);
    evv_frame_done();
    t->state = DEAD;
    run_next();
    coop_fatal("a finished task was resumed");
}

/* Frees what a finished task held. Never its own: a task cannot free the
   stack it is standing on, so this runs from whichever context comes next. */
static void reap(void)
{
    int i;

    for (i = 1; i < MAX_TASKS; i++)
        if (tasks[i].state == DEAD && &tasks[i] != current) {
            free(tasks[i].stack);
            free(tasks[i].tls);
            tasks[i].stack = NULL;
            tasks[i].tls = NULL;
            tasks[i].state = FREE;
        }
}

static int runnable(struct evv_task *t)
{
    return t->state == READY
        || (t->state == BLOCKED && t->timed && (int)(now_ms - t->deadline) >= 0);
}

/* Round robin from the context after the current one; if nothing can run,
   advance the clock to the nearest deadline; if there is none, nothing ever
   will run again, which is a deadlock and is said as one. */
static void run_next(void)
{
    struct evv_task *from = current, *to = NULL;
    int i, start = (int)(from - tasks);

    for (;;) {
        for (i = 1; i <= MAX_TASKS; i++) {
            struct evv_task *t = &tasks[(start + i) % MAX_TASKS];
            if (runnable(t)) {
                to = t;
                break;
            }
        }
        if (to != NULL)
            break;
        {
            int found = 0;
            unsigned best = 0;
            for (i = 0; i < MAX_TASKS; i++)
                if (tasks[i].state == BLOCKED && tasks[i].timed
                    && (!found || (int)(tasks[i].deadline - best) < 0)) {
                    best = tasks[i].deadline;
                    found = 1;
                }
            if (!found)
                coop_fatal("every context is waiting forever");
            now_ms = best;
        }
    }
    if (to->state == BLOCKED) {
        to->state = READY;
        to->waiting_on = NULL;
    }
    if (to == from)
        return;
    current = to;
    coop_tp = to->tls;
    coop_switch(&from->sp, to->sp);
    reap();
}

/* Gives every other ready context a turn before coming back. */
static void yield(void)
{
    run_next();
}

/* Blocks the current context on an object until it is woken or ms runs out.
   The caller rechecks its condition after, so a spurious return is harmless. */
static void block(const void *on, int ms)
{
    current->state = BLOCKED;
    current->waiting_on = on;
    current->timed = ms != EVV_FOREVER;
    current->deadline = now_ms + (unsigned)(ms < 0 ? 0 : ms);
    run_next();
}

static void wake(const void *on)
{
    int i;

    for (i = 0; i < MAX_TASKS; i++)
        if (tasks[i].state == BLOCKED && tasks[i].waiting_on == on) {
            tasks[i].state = READY;
            tasks[i].waiting_on = NULL;
        }
}

void evv_port_start(void)
{
    if (current != NULL)
        return;
    tasks[0].state = READY;
    tasks[0].tls = tls_new();
    if (tasks[0].tls == NULL)
        coop_fatal("no memory for the first context's thread-local block");
    for (int i = 1; i < MAX_TASKS; i++)
        tasks[i].state = FREE;
    current = &tasks[0];
    coop_tp = tasks[0].tls;
}

void evv_port_finish(void) { }

void evv_low_lock(void) { }
void evv_low_unlock(void) { }

evv_sem *evv_sem_create(int initial, int most)
{
    evv_sem *s = malloc(sizeof *s);

    if (s != NULL) {
        s->count = initial;
        s->most = most;
    }
    return s;
}

void evv_sem_destroy(evv_sem *s)
{
    free(s);
}

int evv_sem_wait(evv_sem *s, int ms)
{
    unsigned until = now_ms + (unsigned)(ms < 0 ? 0 : ms);

    if (s == NULL)
        return EVV_WAIT_FAILED;
    while (s->count <= 0) {
        if (ms == 0 || (ms != EVV_FOREVER && (int)(now_ms - until) >= 0))
            return EVV_WAIT_TIMEOUT;
        block(s, ms == EVV_FOREVER ? EVV_FOREVER : (int)(until - now_ms));
    }
    s->count--;
    return EVV_WAIT_OK;
}

int evv_sem_post(evv_sem *s, int n)
{
    if (s == NULL || n < 0)
        return 0;
    if (s->most > 0 && s->count + n > s->most)
        return 0;
    s->count += n;
    wake(s);
    return 1;
}

evv_event *evv_event_create(int signalled)
{
    evv_event *e = malloc(sizeof *e);

    if (e != NULL) {
        e->signalled = signalled != 0;
        e->pulses = 0;
    }
    return e;
}

void evv_event_destroy(evv_event *e)
{
    free(e);
}

int evv_event_wait(evv_event *e, int ms)
{
    unsigned until = now_ms + (unsigned)(ms < 0 ? 0 : ms);
    unsigned seen;

    if (e == NULL)
        return EVV_WAIT_FAILED;
    seen = e->pulses;
    while (!e->signalled && e->pulses == seen) {
        if (ms == 0 || (ms != EVV_FOREVER && (int)(now_ms - until) >= 0))
            return EVV_WAIT_TIMEOUT;
        block(e, ms == EVV_FOREVER ? EVV_FOREVER : (int)(until - now_ms));
    }
    return EVV_WAIT_OK;
}

void evv_event_signal(evv_event *e)
{
    if (e == NULL)
        return;
    e->signalled = 1;
    wake(e);
}

void evv_event_unsignal(evv_event *e)
{
    if (e != NULL)
        e->signalled = 0;
}

/* Lets through whoever is waiting now and leaves it unsignalled: a waiter
   notices the pulse count moved on. */
void evv_event_pulse(evv_event *e)
{
    if (e == NULL)
        return;
    e->pulses++;
    wake(e);
}

evv_task *evv_task_start(void (*entry)(void *), void *arg, int stack_bytes)
{
    struct evv_task *t = NULL;
    uint32_t *sp;
    int i;

    reap();
    for (i = 1; i < MAX_TASKS; i++)
        if (tasks[i].state == FREE) {
            t = &tasks[i];
            break;
        }
    if (t == NULL || entry == NULL)
        return NULL;
    if (stack_bytes < 16384)
        stack_bytes = 16384;
    stack_bytes = (stack_bytes + 7) & ~7;
    t->stack = malloc((size_t)stack_bytes);
    t->tls = tls_new();
    if (t->stack == NULL || t->tls == NULL) {
        free(t->stack);
        free(t->tls);
        t->stack = t->tls = NULL;
        return NULL;
    }
    t->entry = entry;
    t->arg = arg;

    /* What coop_switch pops: d8-d15, then r4-r11, then the pc. */
    sp = (uint32_t *)((char *)t->stack + stack_bytes);
    *--sp = (uint32_t)coop_trampoline | 1u;
    for (i = 0; i < 8; i++)
        *--sp = 0;
    for (i = 0; i < 16; i++)
        *--sp = 0;
    t->sp = (uint32_t)sp;
    t->state = READY;
    t->waiting_on = NULL;
    return t;
}

void evv_task_stop(evv_task *t) { (void)t; }

int evv_task_priority_get(evv_task *t, int *out)
{
    (void)t;
    if (out != NULL)
        *out = 0;
    return 0;
}

int evv_task_priority_set(evv_task *t, int priority)
{
    (void)t;
    (void)priority;
    return 0;
}

unsigned evv_task_self(void)
{
    return (unsigned)(current - tasks) + 1u;
}

void evv_sleep_ms(int ms)
{
    if (ms <= 0) {
        yield();
        return;
    }
    block(NULL, ms);
}

unsigned evv_ticks_ms(void)
{
    return now_ms;
}
