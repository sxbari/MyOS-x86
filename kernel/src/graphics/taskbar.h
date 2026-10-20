#ifndef MYOS_TASKBAR_H
#define MYOS_TASKBAR_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <limine.h>

typedef enum {
    TASKBAR_NONE,
    TASKBAR_REDRAW,
    TASKBAR_CONSOLE,
    TASKBAR_VIEWER,
    TASKBAR_DESKTOP,
    TASKBAR_ABOUT,
    TASKBAR_REBOOT,
    TASKBAR_FILE
} taskbar_action_t;

void taskbar_init(void);
void taskbar_invalidate(void);

bool taskbar_needs_draw(uint64_t uptime_seconds);

void taskbar_draw(
    struct limine_framebuffer *fb,
    uint64_t uptime_seconds,
    bool console_open,
    bool viewer_open,
    bool viewer_front
);

taskbar_action_t taskbar_click(
    struct limine_framebuffer *fb,
    uint64_t x,
    uint64_t y,
    bool viewer_open,
    size_t *file_slot
);

#endif
