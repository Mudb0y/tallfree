/* The way into the engine image, the same under QEMU and on the instrument.

   The image is linked to run at one fixed address and is loaded there whole,
   so the only setup it needs is its own: zero what the linker left as zeroes,
   run the constructors, and move onto a stack of its own. The instrument's
   loader calls engine_start from a firmware task whose stack is not ours to
   size, and the engine wants tens of kilobytes of it. */

#include <stdint.h>
#include <string.h>

extern char __bss_start[], __bss_end[], __stack_top[];
extern void (*__init_array_start[])(void), (*__init_array_end[])(void);

int engine_main(void);
void target_enter(void);
void target_leave(void);

void _init(void) { }
void _fini(void) { }

__asm__(
".syntax unified\n"
".thumb\n"
".text\n"
".globl call_on_stack\n"
".thumb_func\n"
".type call_on_stack, %function\n"
/* call_on_stack(top, fn): runs fn on the stack ending at top and comes back
   to the caller's stack with fn's answer. */
"call_on_stack:\n"
"    push {r4, lr}\n"
"    mov  r4, sp\n"
"    mov  sp, r0\n"
"    blx  r1\n"
"    mov  sp, r4\n"
"    pop  {r4, pc}\n"
);

int call_on_stack(void *top, int (*fn)(void));

static int started;

__attribute__((section(".entry"), used))
int engine_start(void)
{
    if (!started) {
        void (**f)(void);

        memset(__bss_start, 0, (size_t)(__bss_end - __bss_start));
        started = 1;
        for (f = __init_array_start; f < __init_array_end; f++)
            (*f)();
    }
    {
        int rc;

        target_enter();
        rc = call_on_stack(__stack_top, engine_main);
        target_leave();
        return rc;
    }
}
