#ifndef TALLFREE_KERNEL_H
#define TALLFREE_KERNEL_H

/* The instrument's kernel, through the firmware's stubs. Device build only. */

int kernel_task_create(const char *name, void (*entry)(int, void *), void *arg,
                       int priority, void *stack, int stack_size);
int kernel_task_start(int task, int code);
int kernel_sem_create(const char *name, int initial, int maximum);
int kernel_sem_wait(int sem, int timeout_ms);
int kernel_sem_signal(int sem);
int kernel_sem_delete(int sem);
__attribute__((noreturn)) void kernel_task_exit_delete(void);
int kernel_task_self(void);
int kernel_task_priority(int task, int priority);

#endif
