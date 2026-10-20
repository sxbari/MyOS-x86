
#include "desktop_files.h"
#include "graphics.h"
#include "../filesystem/ramfs.h"

#include <stdint.h>
#include <stddef.h>

/* ============== ICON DIMENSIONS ============== */

#define ICON_W 72U
#define ICON_H 78U
#define ICON_PITCH 84U
#define ICON_Y0 18U
#define TASKBAR_RESERVED 46U

/* ============== ICON COLORS ============== */

#define C_TILE_SHADOW 0x006F4C96U
#define C_TILE_EDGE   0x009679C3U
#define C_TILE_BG     0x00F5E9FFU

#define C_PAGE_EDGE   0x006E509AU
#define C_PAGE_BG     0x00FFFDFEU
#define C_PAGE_FOLD   0x00D7BBF0U
#define C_PAGE_LINE   0x00BA9BDCU

#define C_TEXT        0x00452E66U

/* ============== REDRAW STATE ============== */

static bool dirty = true;

void desktop_files_mark_dirty(void)
{
    dirty = true;
}

bool desktop_files_needs_redraw(void)
{
    return dirty;
}

void desktop_files_clear_dirty(void)
{
    dirty = false;
}

/* ============== FILENAME FONT ============== */

/* 5x7 font. Lowercase displays as uppercase. */

static const uint8_t letters[36][7] = {
    {14,17,17,31,17,17,17},  // A
    {30,17,17,30,17,17,30},  // B
    {15,16,16,16,16,16,15},  // C
    {30,17,17,17,17,17,30},  // D
    {31,16,16,30,16,16,31},  // E
    {31,16,16,30,16,16,16},  // F
    {15,16,16,23,17,17,15},  // G
    {17,17,17,31,17,17,17},  // H
    {31,4,4,4,4,4,31},       // I
    {7,2,2,2,18,18,12},      // J
    {17,18,20,24,20,18,17}, // K
    {16,16,16,16,16,16,31}, // L
    {17,27,21,21,17,17,17}, // M
    {17,25,21,19,17,17,17}, // N
    {14,17,17,17,17,17,14}, // O
    {30,17,17,30,16,16,16}, // P
    {14,17,17,17,21,18,13}, // Q
    {30,17,17,30,20,18,17}, // R
    {15,16,16,14,1,1,30},   // S
    {31,4,4,4,4,4,4},       // T
    {17,17,17,17,17,17,14}, // U
    {17,17,17,17,17,10,4},  // V
    {17,17,17,21,21,21,10}, // W
    {17,17,10,4,10,17,17},  // X
    {17,17,10,4,4,4,4},     // Y
    {31,1,2,4,8,16,31},    // Z

    {14,17,19,21,25,17,14}, // 0
    {4,12,4,4,4,4,14},       // 1
    {14,17,1,2,4,8,31},      // 2
    {30,1,1,6,1,17,14},      // 3
    {2,6,10,18,31,2,2},     // 4
    {31,16,30,1,1,17,14},   // 5
    {14,16,30,17,17,17,14}, // 6
    {31,1,2,4,8,8,8},       // 7
    {14,17,17,14,17,17,14}, // 8
    {14,17,17,15,1,1,14}    // 9
};

/* ============== FONT CHARACTER LOOKUP ============== */

static uint8_t glyph_row(char ch, uint32_t row)
{
    if (ch >= 'a' && ch <= 'z')
        ch -= ('a' - 'A');

    if (ch >= 'A' && ch <= 'Z')
        return letters[ch - 'A'][row];

    if (ch >= '0' && ch <= '9')
        return letters[26 + ch - '0'][row];

    if (ch == '.' && row == 6)
        return 4;

    if (ch == '_' && row == 6)
        return 31;

    if (ch == '-' && row == 3)
        return 14;

    if (ch == '/')
        return (uint8_t)(1U << (row < 5 ? row : 4));

    return 0;
}

/* ============== DRAW A FILENAME CHARACTER ============== */

static void draw_name_char(
    struct limine_framebuffer *fb,
    uint64_t x,
    uint64_t y,
    char ch
)
{
    for (uint32_t row = 0; row < 7; row++) {
        uint8_t bits = glyph_row(ch, row);

        for (uint32_t col = 0; col < 5; col++) {
            if (bits & (1U << (4 - col))) {
                graphics_pixel(
                    fb,
                    x + col,
                    y + row,
                    C_TEXT
                );
            }
        }
    }
}

/* ============== FILENAME LENGTH ============== */

static size_t name_length(const char *s)
{
    size_t length = 0;

    while (s[length] && length < 256)
        length++;

    return length;
}

/* ============== DRAW FILENAME ============== */

static void draw_filename(
    struct limine_framebuffer *fb,
    uint64_t x,
    uint64_t y,
    const char *name
)
{
    size_t len = name_length(name);
    size_t shown = len < 20 ? len : 20;

    for (size_t line = 0; line < 2; line++) {
        size_t offset = line * 10;

        if (shown <= offset)
            break;

        size_t count = shown - offset;

        if (count > 10)
            count = 10;

        uint64_t text_x =
            x + (ICON_W - count * 6 + 1) / 2;

        for (size_t i = 0; i < count; i++) {
            char ch = name[offset + i];

            /* Show an ellipsis for long names. */
            if (len > 20 && line == 1 && i >= 7)
                ch = '.';

            draw_name_char(
                fb,
                text_x + i * 6,
                y + 55 + line * 9,
                ch
            );
        }
    }
}

/* ============== DRAW FILE ICON ============== */

static void draw_icon(
    struct limine_framebuffer *fb,
    uint64_t x,
    uint64_t y,
    const char *name
)
{
    /* Shadow behind the icon tile. */
    graphics_rect(
        fb,
        x + 2, y + 3,
        ICON_W - 2, ICON_H - 3,
        C_TILE_SHADOW
    );

    /* Lavender tile border. */
    graphics_rect(
        fb,
        x, y,
        ICON_W - 2, ICON_H - 3,
        C_TILE_EDGE
    );

    /* Pastel tile background. */
    graphics_rect(
        fb,
        x + 1, y + 1,
        ICON_W - 4, ICON_H - 5,
        C_TILE_BG
    );

    /* Document icon position. */
    uint64_t px = x + 21;
    uint64_t py = y + 6;

    /* Document border and white paper. */
    graphics_rect(
        fb, px, py,
        31, 42,
        C_PAGE_EDGE
    );

    graphics_rect(
        fb, px + 2, py + 2,
        27, 38,
        C_PAGE_BG
    );

    /* Folded upper-right corner. */
    graphics_rect(
        fb, px + 22, py + 1,
        8, 8,
        C_TILE_BG
    );

    graphics_rect(
        fb, px + 22, py + 9,
        8, 1,
        C_PAGE_EDGE
    );

    graphics_rect(
        fb, px + 22, py + 2,
        1, 7,
        C_PAGE_EDGE
    );

    graphics_rect(
        fb, px + 23, py + 2,
        6, 6,
        C_PAGE_FOLD
    );

    /* Lines inside the paper. */
    graphics_rect(
        fb, px + 6, py + 17,
        18, 2,
        C_PAGE_LINE
    );

    graphics_rect(
        fb, px + 6, py + 23,
        17, 2,
        C_PAGE_LINE
    );

    graphics_rect(
        fb, px + 6, py + 29,
        14, 2,
        C_PAGE_LINE
    );

    /* Draw the filename below the paper. */
    draw_filename(fb, x, y, name);
}

/* ============== ICON POSITIONING ============== */

/*
 * Icons occupy the margins beside the console.
 * This keeps the console text and taskbar visible.
 */

static bool position_for(
    struct limine_framebuffer *fb,
    size_t ordinal,
    uint64_t *out_x,
    uint64_t *out_y
)
{
    if (!fb ||
        fb->height <=
        ICON_Y0 + ICON_H + TASKBAR_RESERVED)
        return false;

    gfx_rect_t console = graphics_console_rect(fb);

    uint64_t max_y =
        fb->height - TASKBAR_RESERVED;

    uint64_t rows =
        (max_y - ICON_Y0) / ICON_PITCH;

    if (!rows)
        return false;

    /* Left margin beside the console. */
    uint64_t left_cols =
        console.x >= ICON_W + 6 ? 1 : 0;

    /* Right margin beside the console. */
    uint64_t right_edge =
        console.x + console.width;

    uint64_t right_space =
        right_edge < fb->width
        ? fb->width - right_edge
        : 0;

    uint64_t right_cols =
        right_space >= ICON_W + 12
        ? (right_space - 12) / ICON_PITCH
        : 0;

    if (right_cols > 2)
        right_cols = 2;

    uint64_t total_cols = left_cols + right_cols;

    if (!total_cols ||
        ordinal >= rows * total_cols)
        return false;

    uint64_t col = ordinal / rows;
    uint64_t row = ordinal % rows;

    uint64_t x;

    if (col < left_cols) {
        x = (console.x - ICON_W) / 2;
    }
    else {
        uint64_t right_col = col - left_cols;

        uint64_t span =
            right_cols * ICON_PITCH;

        uint64_t offset =
            (right_space - span) / 2;

        x = right_edge + offset +
            right_col * ICON_PITCH + 6;
    }

    uint64_t y =
        ICON_Y0 + row * ICON_PITCH;

    if (x + ICON_W > fb->width ||
        y + ICON_H > max_y)
        return false;

    *out_x = x;
    *out_y = y;

    return true;
}

/* ============== DRAW ALL RAMFS ICONS ============== */

void desktop_files_draw(
    struct limine_framebuffer *fb
)
{
    if (!fb || fb->bpp != 32)
        return;

    size_t ordinal = 0;

    for (size_t i = 0; i < RAMFS_MAX_FILES; i++) {

        const char *name = ramfs_name(i);

        if (!name)
            continue;

        uint64_t x;
        uint64_t y;

        if (!position_for(
                fb, ordinal, &x, &y))
            break;

        draw_icon(fb, x, y, name);

        ordinal++;
    }
}
/* ============== FILE ICON HIT DETECTION ============== */

const char *desktop_files_hit_test(
    struct limine_framebuffer *fb,
    uint64_t mouse_x,
    uint64_t mouse_y
)
{
    if (!fb)
        return NULL;

    size_t ordinal = 0;

    for (size_t i = 0; i < RAMFS_MAX_FILES; i++) {

        const char *name = ramfs_name(i);

        if (!name)
            continue;

        uint64_t x, y;

        if (!position_for(fb, ordinal, &x, &y))
            break;

        if (mouse_x >= x &&
            mouse_x < x + ICON_W &&
            mouse_y >= y &&
            mouse_y < y + ICON_H) {

            return name;
        }

        ordinal++;
    }

    return NULL;
}
