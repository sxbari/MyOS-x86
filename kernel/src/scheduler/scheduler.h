
#ifndef MYOS_SCHEDULER_H
#define MYOS_SCHEDULER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define MAX_TASKS 3
#define TASK_STACK_SIZE 16384
#define TIME_QUANTUM 5

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_TERMINATED
} task_state_t;

typedef struct {
    uint32_t pid;
    task_state_t state;

    uint64_t rsp;
    void *stack_base;

    uint32_t ticks_used;

    void (*entry)(void);
} task_t;

void scheduler_init(void);
int task_create(void (*entry)(void));
uint32_t scheduler_task_count(void);
task_t *scheduler_get_task(uint32_t index);


uint64_t scheduler_on_tick(uint64_t interrupted_rsp);
void scheduler_set_demo_running(bool running);
#endif
