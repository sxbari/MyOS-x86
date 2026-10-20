
#ifndef MYOS_GRAPHICS_H
#define MYOS_GRAPHICS_H

#include <stdint.h>
#include <stdbool.h>
#include <limine.h>

typedef struct {
    uint64_t x, y, width, height;
} gfx_rect_t;

typedef enum {
    GFX_CONSOLE_OPEN,
    GFX_CONSOLE_MINIMIZED,
    GFX_CONSOLE_CLOSED
} gfx_console_state_t;

void graphics_pixel(struct limine_framebuffer *fb,
                    uint64_t x, uint64_t y, uint32_t color);

void graphics_rect(struct limine_framebuffer *fb,
                   uint64_t x, uint64_t y,
                   uint64_t width, uint64_t height,
                   uint32_t color);

void graphics_text(struct limine_framebuffer *fb,
                   uint64_t x, uint64_t y,
                   const char *text, uint32_t color);

void graphics_wallpaper(struct limine_framebuffer *fb);
void graphics_draw_desktop(struct limine_framebuffer *fb);
void graphics_clear_console_body(struct limine_framebuffer *fb);

void graphics_set_console_state(gfx_console_state_t state);
gfx_console_state_t graphics_get_console_state(void);

gfx_rect_t graphics_console_rect(struct limine_framebuffer *fb);
gfx_rect_t graphics_close_button_rect(struct limine_framebuffer *fb);
gfx_rect_t graphics_minimize_button_rect(struct limine_framebuffer *fb);
gfx_rect_t graphics_taskbar_button_rect(struct limine_framebuffer *fb);

bool graphics_point_in_rect(gfx_rect_t r, uint64_t x, uint64_t y);

#endif
