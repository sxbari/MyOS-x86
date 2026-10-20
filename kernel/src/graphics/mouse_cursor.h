#ifndef MYOS_MOUSE_CURSOR_H
#define MYOS_MOUSE_CURSOR_H

#include <stdbool.h>
#include <stdint.h>
#include <limine.h>

void mouse_cursor_init(
    struct limine_framebuffer *fb
);

void mouse_cursor_process(
    struct limine_framebuffer *fb
);

void mouse_cursor_hide(
    struct limine_framebuffer *fb
);

void mouse_cursor_show(
    struct limine_framebuffer *fb
);

uint64_t mouse_left_clicks(void);

void mouse_cursor_position(
    uint64_t *x,
    uint64_t *y
);
bool mouse_cursor_pop_click(
    uint64_t *x,
    uint64_t *y
);
bool mouse_cursor_overlaps(
    uint64_t x,
    uint64_t y,
    uint64_t width,
    uint64_t height
);
#endif