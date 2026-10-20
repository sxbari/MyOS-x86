#ifndef MYOS_COMPANION_H
#define MYOS_COMPANION_H
#include <stdbool.h>
#include <stdint.h>
#include <limine.h>
#include "graphics.h"

typedef enum {
    COMPANION_TALK,
    COMPANION_HAPPY
} companion_reaction_t;

void companion_init(uint64_t now_ticks);
void companion_react(companion_reaction_t reaction, uint64_t now_ticks);
/* Returns true when the companion's animation frame changes. */
bool companion_update(
    uint64_t now_ticks,
    bool console_open
);
gfx_rect_t companion_bounds(
    struct limine_framebuffer *fb,
    bool console_open
);
void companion_restore_background(struct limine_framebuffer *fb);
void companion_draw(struct limine_framebuffer *fb);

#endif
