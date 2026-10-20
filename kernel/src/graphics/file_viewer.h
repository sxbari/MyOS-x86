#ifndef MYOS_FILE_VIEWER_H
#define MYOS_FILE_VIEWER_H

#include <stdbool.h>
#include <stdint.h>
#include <limine.h>
#include "graphics.h"

void file_viewer_init(void);
gfx_rect_t file_viewer_bounds(struct limine_framebuffer *fb);

bool file_viewer_open(const char *name);
void file_viewer_close(void);

bool file_viewer_is_open(void);
bool file_viewer_displays(const char *name);

void file_viewer_mark_dirty(void);
bool file_viewer_needs_redraw(void);

bool file_viewer_handle_click(
    struct limine_framebuffer *fb,
    uint64_t x,
    uint64_t y
);

void file_viewer_draw(
    struct limine_framebuffer *fb
);

#endif
