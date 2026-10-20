
#ifndef MYOS_DESKTOP_FILES_H
#define MYOS_DESKTOP_FILES_H

#include <stdbool.h>
#include <limine.h>
#include <stdint.h>

/* Notify desktop after a file is created or deleted. */
void desktop_files_mark_dirty(void);

/* Check whether icons need a redraw. */
bool desktop_files_needs_redraw(void);

/* Mark desktop icon drawing as completed. */
void desktop_files_clear_dirty(void);

/* Draw RAMFS file icons on the desktop. */
void desktop_files_draw(
    struct limine_framebuffer *fb
);
const char *desktop_files_hit_test(
    struct limine_framebuffer *fb,
    uint64_t mouse_x,
    uint64_t mouse_y
);
#endif
