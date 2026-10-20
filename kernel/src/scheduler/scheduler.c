
#include "scheduler.h"
#include "../memory/heap.h"

static task_t tasks[MAX_TASKS];
static uint32_t task_count = 0;

// -1 means the original kernel/idle context.
static int current_task = -1;

static uint64_t main_rsp = 0;
static uint32_t slice_ticks = 0;

// Called only if a task unexpectedly returns.
__attribute__((noreturn))
static void task_return_trap(void)
{
    for (;;) {
        asm volatile ("sti; hlt" ::: "memory");
    }
}

void scheduler_init(void)
{
    task_count = 0;
    current_task = -1;
    main_rsp = 0;
    slice_ticks = 0;

    for (uint32_t i = 0; i < MAX_TASKS; i++) {
        tasks[i] = (task_t){0};
    }
}
void scheduler_set_demo_running(bool running)
{
    for (uint32_t i = 0; i < task_count; i++) {
        if (running) {
            tasks[i].state =
                ((int)i == current_task)
                ? TASK_RUNNING : TASK_READY;
        } else {
            tasks[i].state = TASK_BLOCKED;
        }
    }
}
int task_create(void (*entry)(void))
{
    if (!entry || task_count >= MAX_TASKS)
        return -1;

    void *stack = kmalloc(TASK_STACK_SIZE);

    if (!stack)
        return -1;

    task_t *task = &tasks[task_count];

    task->pid = task_count + 1;
    task->state = TASK_BLOCKED;
    task->stack_base = stack;
    task->entry = entry;
    task->ticks_used = 0;

    uint64_t stack_top =
        ((uint64_t)stack + TASK_STACK_SIZE) & ~15ULL;

    uint64_t cs;
    uint64_t ss;
    asm volatile ("mov %%cs, %0" : "=r"(cs));
    asm volatile ("mov %%ss, %0" : "=r"(ss));

    uint64_t *sp = (uint64_t *)stack_top;

    // Return address if the task function finishes.
    *(--sp) = (uint64_t)task_return_trap;
    uint64_t initial_rsp = (uint64_t)sp;

    // Match the full 64-bit IRETQ frame: RIP, CS, RFLAGS, RSP, SS.
    *(--sp) = ss;
    *(--sp) = initial_rsp;
    *(--sp) = 0x202;
    *(--sp) = cs;
    *(--sp) = (uint64_t)entry;

    // Space for the 15 general-purpose registers restored by interrupt_stub.
    for (int i = 0; i < 15; i++)
        *(--sp) = 0;

    task->rsp = (uint64_t)sp;

    task_count++;
    return (int)task->pid;
}


uint64_t scheduler_on_tick(uint64_t interrupted_rsp)
{
    if (task_count == 0)
        return interrupted_rsp;

    // Preserve the context interrupted by the PIT.
    if (current_task == -1)
        main_rsp = interrupted_rsp;
    else
        tasks[current_task].rsp = interrupted_rsp;

    slice_ticks++;

    // Continue the current time slice unless the task
    // has been blocked or its quantum has expired.
    bool still_runnable =
        current_task == -1 ||
        tasks[current_task].state == TASK_RUNNING;

    if (still_runnable && slice_ticks < TIME_QUANTUM)
        return interrupted_rsp;

    slice_ticks = 0;

    if (current_task >= 0 &&
        tasks[current_task].state == TASK_RUNNING) {
        tasks[current_task].state = TASK_READY;
    }

    int next = current_task;

    // Search in Round Robin order.
    // The main kernel (-1) always remains runnable.
    for (uint32_t i = 0; i <= task_count; i++) {
        next++;

        if (next >= (int)task_count)
            next = -1;

        if (next == -1) {
            current_task = -1;
            return main_rsp;
        }

        if (tasks[next].state == TASK_READY) {
            current_task = next;
            tasks[next].state = TASK_RUNNING;
            return tasks[next].rsp;
        }
    }

    current_task = -1;
    return main_rsp;
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
