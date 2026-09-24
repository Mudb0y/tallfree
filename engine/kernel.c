/* The instrument's kernel services, called through the firmware's own stubs.

   Addresses and record layouts are the ones recorded in notes.md under "The
   kernel calls the resident engine needs". Timeouts are milliseconds, -1
   meaning wait for ever; a negative return is the kernel's error code. */

#include <stdint.h>
#include <string.h>
#include "kernel.h"

struct task_record {
    void    *arg;
    uint32_t attributes;
    void   (*entry)(int code, void *arg);
    int32_t  priority;
    int32_t  stack_size;
    void    *buffer;
    char     name[8];
};

struct sem_record {
    void    *arg;
    uint32_t attributes;
    int32_t  initial;
    int32_t  maximum;
    char     name[8];
};

typedef int (*create_task_fn)(const struct task_record *);
typedef int (*start_task_fn)(int task, int code);
typedef int (*create_sem_fn)(const struct sem_record *);
typedef int (*wait_sem_fn)(int sem, int count, int timeout);
typedef int (*signal_sem_fn)(int sem, int count);

#define CREATE_TASK ((create_task_fn)0x800D31A9u)
#define START_TASK  ((start_task_fn) 0x800D31B3u)
#define CREATE_SEM  ((create_sem_fn) 0x800D30FFu)
#define WAIT_SEM    ((wait_sem_fn)   0x800D3253u)
#define SIGNAL_SEM  ((signal_sem_fn) 0x800D3249u)

/* High-level language, named, uses the FPU: what the firmware's own tasks
   carry. The engine's arithmetic is mostly fixed point, but the C library
   and the hard-float calling convention touch the FPU registers. */
#define TASK_ATTRIBUTES 0x1041u
#define SEM_ATTRIBUTES  0x0040u              /* named, first-in first-out */

int kernel_task_create(const char *name, void (*entry)(int, void *), void *arg,
                       int priority, int stack_size)
{
    struct task_record r;

    memset(&r, 0, sizeof r);
    r.arg = arg;
    r.attributes = TASK_ATTRIBUTES;
    r.entry = entry;
    r.priority = priority;
    r.stack_size = stack_size;
    strncpy(r.name, name, sizeof r.name);
    return CREATE_TASK(&r);
}

int kernel_task_start(int task, int code)
{
    return START_TASK(task, code);
}

int kernel_sem_create(const char *name, int initial, int maximum)
{
    struct sem_record r;

    memset(&r, 0, sizeof r);
    r.attributes = SEM_ATTRIBUTES;
    r.initial = initial;
    r.maximum = maximum;
    strncpy(r.name, name, sizeof r.name);
    return CREATE_SEM(&r);
}

int kernel_sem_wait(int sem, int timeout_ms)
{
    return WAIT_SEM(sem, 1, timeout_ms);
}

int kernel_sem_signal(int sem)
{
    return SIGNAL_SEM(sem, 1);
}
