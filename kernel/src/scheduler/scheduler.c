
#include "scheduler.h"
#include "../memory/heap.h"

static task_t tasks[MAX_TASKS];
static uint32_t task_count = 0;

void scheduler_init(void)
{
    task_count = 0;

    for (uint32_t i = 0; i < MAX_TASKS; i++) {
        tasks[i] = (task_t){0};
    }
}

int task_create(void (*entry)(void))
{
    if (entry == NULL || task_count >= MAX_TASKS)
        return -1;

    void *stack = kmalloc(TASK_STACK_SIZE);

    if (stack == NULL)
        return -1;

    task_t *task = &tasks[task_count];

    task->pid = task_count + 1;
    task->state = TASK_READY;

    task->stack_base = stack;
    task->ticks_used = 0;
    task->entry = entry;

    // Top of stack, aligned to 16 bytes.
    uint64_t stack_top =
        (uint64_t)stack + TASK_STACK_SIZE;

    stack_top &= ~((uint64_t)15);

    task->rsp = stack_top;

    task_count++;

    return (int)task->pid;
}

uint32_t scheduler_task_count(void)
{
    return task_count;
}

task_t *scheduler_get_task(uint32_t index)
{
    if (index >= task_count)
        return NULL;

    return &tasks[index];
}
