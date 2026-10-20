#include "mouse_cursor.h"
#include "../interrupts/mouse.h"

#include <stdbool.h>

#define CURSOR_WIDTH 20
#define CURSOR_HEIGHT 24
#define MOUSE_SPEED 2


/*
 * O = dark outline
 * P = deep magenta
 * C = deep teal
 * Y = amber
 * W = white highlight
 * . = transparent
 */

static const char sprite
    [CURSOR_HEIGHT][CURSOR_WIDTH + 1] = {

    "O...................",
    "OO..................",
    "OWO.................",
    "OWWO................",
    "OWPWO...............",
    "OWPPWO..............",
    "OWPPPCO.............",
    "OWPPPPCO............",
    "OWPPPPPCO...........",
    "OWPPPPPPPCO.........",
    "OWPPPPPPPPPCO.......",
    "OWPPPPPPPPPPCO......",
    "OWPPPPPPPPPPPCO.....",
    "OWPPPPPPPPPPPPYOO...",
    "OWPPPPPPPPPYYOO.....",
    "OWPPPPPPPPYYO.......",
    "OWPPPPPPYYO.........",
    "OWPPPPYYO...........",
    "OWPWO...OO..........",
    "OWPPWO..O...........",
    "OWPPPWO.O...........",
    "OOYPPPCOO...........",
    ".OOWPPPCO...........",
    "..OO..OO............"
};

static uint32_t background
    [CURSOR_HEIGHT][CURSOR_WIDTH];

static uint64_t pos_x;
static uint64_t pos_y;

static uint8_t buttons;
static uint64_t click_count;

static bool visible;

#define CLICK_QUEUE_SIZE 16

typedef struct {
    uint64_t x;
    uint64_t y;
} cursor_click_t;

static cursor_click_t click_queue[CLICK_QUEUE_SIZE];

static uint32_t click_head = 0;
static uint32_t click_tail = 0;

static void save_click(void)
{
    uint32_t next =
        (click_head + 1) % CLICK_QUEUE_SIZE;

    if (next == click_tail)
        return;

    click_queue[click_head].x = pos_x;
    click_queue[click_head].y = pos_y;

    click_head = next;
}

bool mouse_cursor_pop_click(
    uint64_t *x,
    uint64_t *y
)
{
    if (click_tail == click_head)
        return false;

    if (x)
        *x = click_queue[click_tail].x;

    if (y)
        *y = click_queue[click_tail].y;

    click_tail =
        (click_tail + 1) % CLICK_QUEUE_SIZE;

    return true;
}

bool mouse_cursor_overlaps(
    uint64_t x,
    uint64_t y,
    uint64_t width,
    uint64_t height
)
{
    if (!visible || !width || !height)
        return false;

    return pos_x < x + width &&
           x < pos_x + CURSOR_WIDTH &&
           pos_y < y + height &&
           y < pos_y + CURSOR_HEIGHT;
}

/* =============== INITIALIZATION =============== */

void mouse_cursor_init(
    struct limine_framebuffer *fb
)
{
    click_head = 0;
    click_tail = 0;
    visible = false;
    buttons = 0;
    click_count = 0;

    pos_x = fb ? fb->width / 2 : 0;
    pos_y = fb ? fb->height / 2 : 0;
}

/* =============== MOUSE INFORMATION =============== */

uint64_t mouse_left_clicks(void)
{
    return click_count;
}

void mouse_cursor_position(
    uint64_t *x,
    uint64_t *y
)
{
    if (x)
        *x = pos_x;

    if (y)
        *y = pos_y;
}

/* =============== ERASE OLD CURSOR =============== */

void mouse_cursor_hide(
    struct limine_framebuffer *fb
)
{
    if (!visible || !fb || fb->bpp != 32)
        return;

    uint32_t *pixels = fb->address;
    uint64_t pitch = fb->pitch / 4;

    for (uint64_t y = 0; y < CURSOR_HEIGHT; y++) {
        for (uint64_t x = 0; x < CURSOR_WIDTH; x++) {

            if (pos_x + x < fb->width &&
                pos_y + y < fb->height) {

                pixels[
                    (pos_y + y) * pitch +
                    pos_x + x
                ] = background[y][x];
            }
        }
    }

    visible = false;
}

/* =============== PROCESS MOVEMENT =============== */

void mouse_cursor_process(
    struct limine_framebuffer *fb
)
{
    if (!fb || !fb->width || !fb->height)
        return;

    mouse_event_t event;

    while (mouse_poll(&event)) {

        int64_t nx =
            (int64_t)pos_x + (int64_t)event.dx * MOUSE_SPEED;

        // PS/2 Y is positive upwards.
        int64_t ny =
            (int64_t)pos_y - (int64_t)event.dy * MOUSE_SPEED;

        // Keep pointer inside the framebuffer.
        if (nx < 0)
            nx = 0;

        if (ny < 0)
            ny = 0;

        if (nx >= (int64_t)fb->width)
            nx = (int64_t)fb->width - 1;

        if (ny >= (int64_t)fb->height)
            ny = (int64_t)fb->height - 1;

        uint8_t new_buttons = event.buttons;

        if ((uint64_t)nx != pos_x ||
            (uint64_t)ny != pos_y ||
            new_buttons != buttons) {
            mouse_cursor_hide(fb);
        }

        pos_x = (uint64_t)nx;
        pos_y = (uint64_t)ny;

        // Count each new left-button press.
        if ((new_buttons & 1) &&
            !(buttons & 1)) {
            click_count++;
            save_click();
        }

        buttons = new_buttons;
    }
}

/* =============== DRAW NEW CURSOR =============== */

void mouse_cursor_show(
    struct limine_framebuffer *fb
)
{
    if (visible || !fb || fb->bpp != 32)
        return;

    uint32_t *pixels = fb->address;
    uint64_t pitch = fb->pitch / 4;

    for (uint64_t y = 0; y < CURSOR_HEIGHT; y++) {
        for (uint64_t x = 0; x < CURSOR_WIDTH; x++) {

            if (pos_x + x >= fb->width ||
                pos_y + y >= fb->height) {
                continue;
            }

            uint64_t offset =
                (pos_y + y) * pitch +
                pos_x + x;

            // Save the original framebuffer pixel.
            background[y][x] = pixels[offset];

            switch (sprite[y][x]) {
                case 'O':
                    pixels[offset] = 0x001A1028;
                    break;
                case 'P':
                    pixels[offset] = (buttons & 1)
                        ? 0x00B53372 : 0x00D84786;
                    break;
                case 'C':
                    pixels[offset] = 0x0039B7C8;
                    break;
                case 'Y':
                    pixels[offset] = 0x00E0B23D;
                    break;
                case 'W':
                    pixels[offset] = 0x00FFFFFF;
                    break;
            }
        }
    }

    visible = true;
}