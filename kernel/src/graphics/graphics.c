
#include "graphics.h"

#define COLOR_CONSOLE 0x0030104F
#define COLOR_BORDER  0x00F5DFF4
#define COLOR_BAR     0x006B558D

static gfx_console_state_t console_state = GFX_CONSOLE_OPEN;

/* ================= BASIC DRAWING ================= */

void graphics_pixel(
    struct limine_framebuffer *fb,
    uint64_t x,
    uint64_t y,
    uint32_t color
)
{
    if (!fb || fb->bpp != 32 ||
        x >= fb->width || y >= fb->height)
        return;

    uint32_t *pixels = (uint32_t *)fb->address;
    pixels[y * (fb->pitch / 4) + x] = color;
}

void graphics_rect(
    struct limine_framebuffer *fb,
    uint64_t x,
    uint64_t y,
    uint64_t width,
    uint64_t height,
    uint32_t color
)
{
    if (!fb || fb->bpp != 32 ||
        x >= fb->width || y >= fb->height)
        return;

    if (width > fb->width - x)
        width = fb->width - x;

    if (height > fb->height - y)
        height = fb->height - y;

    uint32_t *pixels = (uint32_t *)fb->address;
    uint64_t stride = fb->pitch / 4;

    for (uint64_t row = y; row < y + height; row++) {
        for (uint64_t col = x; col < x + width; col++) {
            pixels[row * stride + col] = color;
        }
    }
}

/* ================= SMALL UI FONT ================= */

// Five-pixel-wide glyphs used for window titles
// and the taskbar. The shell keeps its existing font.

static const uint8_t glyphs[36][5] = {
    {0x0E,0x11,0x1F,0x11,0x11}, // A
    {0x1E,0x11,0x1E,0x11,0x1E}, // B
    {0x0F,0x10,0x10,0x10,0x0F}, // C
    {0x1E,0x11,0x11,0x11,0x1E}, // D
    {0x1F,0x10,0x1E,0x10,0x1F}, // E
    {0x1F,0x10,0x1E,0x10,0x10}, // F
    {0x0F,0x10,0x17,0x11,0x0F}, // G
    {0x11,0x11,0x1F,0x11,0x11}, // H
    {0x1F,0x04,0x04,0x04,0x1F}, // I
    {0x01,0x01,0x01,0x11,0x0E}, // J
    {0x11,0x12,0x1C,0x12,0x11}, // K
    {0x10,0x10,0x10,0x10,0x1F}, // L
    {0x11,0x1B,0x15,0x11,0x11}, // M
    {0x11,0x19,0x15,0x13,0x11}, // N
    {0x0E,0x11,0x11,0x11,0x0E}, // O
    {0x1E,0x11,0x1E,0x10,0x10}, // P
    {0x0E,0x11,0x15,0x12,0x0D}, // Q
    {0x1E,0x11,0x1E,0x12,0x11}, // R
    {0x0F,0x10,0x0E,0x01,0x1E}, // S
    {0x1F,0x04,0x04,0x04,0x04}, // T
    {0x11,0x11,0x11,0x11,0x0E}, // U
    {0x11,0x11,0x11,0x0A,0x04}, // V
    {0x11,0x11,0x15,0x1B,0x11}, // W
    {0x11,0x0A,0x04,0x0A,0x11}, // X
    {0x11,0x0A,0x04,0x04,0x04}, // Y
    {0x1F,0x02,0x04,0x08,0x1F}, // Z
    {0x0E,0x11,0x15,0x11,0x0E}, // 0
    {0x04,0x0C,0x04,0x04,0x0E}, // 1
    {0x0E,0x11,0x02,0x04,0x1F}, // 2
    {0x1E,0x01,0x06,0x01,0x1E}, // 3
    {0x02,0x06,0x0A,0x1F,0x02}, // 4
    {0x1F,0x10,0x1E,0x01,0x1E}, // 5
    {0x0E,0x10,0x1E,0x11,0x0E}, // 6
    {0x1F,0x01,0x02,0x04,0x08}, // 7
    {0x0E,0x11,0x0E,0x11,0x0E}, // 8
    {0x0E,0x11,0x0F,0x01,0x0E}  // 9
};

void graphics_text(
    struct limine_framebuffer *fb,
    uint64_t x,
    uint64_t y,
    const char *text,
    uint32_t color
)
{
    if (!text)
        return;

    for (; *text; text++, x += 6) {
        char c = *text;

        if (c >= 'a' && c <= 'z')
            c -= 32;

        int index = -1;

        if (c >= 'A' && c <= 'Z')
            index = c - 'A';

        if (c >= '0' && c <= '9')
            index = 26 + c - '0';

        if (index >= 0) {
            for (uint64_t row = 0; row < 5; row++) {
                for (uint64_t col = 0; col < 5; col++) {
                    if (glyphs[index][row] & (1U << (4 - col)))
                        graphics_pixel(
                            fb, x + col, y + row, color
                        );
                }
            }
        }
        else if (c == '_' || c == '-') {
            graphics_rect(
                fb, x, y + (c == '_' ? 5 : 3),
                5, 1, color
            );
        }
        else if (c == '.') {
            graphics_pixel(fb, x + 2, y + 4, color);
        }
    }
}

/* ================= WALLPAPER ================= */

static void circle(
    struct limine_framebuffer *fb,
    int64_t cx,
    int64_t cy,
    int64_t radius,
    uint32_t color
)
{
    for (int64_t y = -radius; y <= radius; y++) {
        for (int64_t x = -radius; x <= radius; x++) {
            int64_t px = cx + x;
            int64_t py = cy + y;

            if (x*x + y*y <= radius*radius &&
                px >= 0 && py >= 0) {

                graphics_pixel(
                    fb, (uint64_t)px, (uint64_t)py, color
                );
            }
        }
    }
}

void graphics_wallpaper(struct limine_framebuffer *fb)
{
    if (!fb || fb->bpp != 32 || fb->height == 0)
        return;

    uint64_t d = fb->height > 1 ? fb->height - 1 : 1;

    // Purple-to-pink vertical gradient.
    for (uint64_t y = 0; y < fb->height; y++) {
        uint32_t r = (118 * (d-y) + 235 * y) / d;
        uint32_t g = ( 90 * (d-y) + 178 * y) / d;
        uint32_t b = (166 * (d-y) + 210 * y) / d;

        graphics_rect(
            fb, 0, y, fb->width, 1,
            (r << 16) | (g << 8) | b
        );
    }

    // Decorative circles; future narrator can
    // occupy this right-hand side.
    if (fb->width > 600 && fb->height > 300) {
        circle(
            fb,
            (int64_t)fb->width - 113,
            (int64_t)fb->height / 3,
            78, 0x00F5CDD9
        );

        circle(
            fb,
            (int64_t)fb->width - 98,
            (int64_t)fb->height / 3 - 12,
            48, 0x00F8DEE5
        );
    }

    // Decorative pixel-art sparkles.
    if (fb->width > 450 && fb->height > 220) {
        const uint64_t marks[5][2] = {
            {26, 38},
            {65, 190},
            {fb->width - 53, 48},
            {fb->width - 190, 125},
            {fb->width / 2, fb->height - 75}
        };

        for (int i = 0; i < 5; i++) {
            uint64_t x = marks[i][0];
            uint64_t y = marks[i][1];

            graphics_rect(
                fb, x + 3, y, 2, 10, 0x00FFF3F9
            );

            graphics_rect(
                fb, x, y + 4, 8, 2, 0x00FFF3F9
            );
        }
    }
}

/* ================= WINDOW GEOMETRY ================= */

static gfx_rect_t make_rect(
    uint64_t x, uint64_t y,
    uint64_t w, uint64_t h
)
{
    gfx_rect_t rect = {x, y, w, h};
    return rect;
}

gfx_rect_t graphics_console_rect(
    struct limine_framebuffer *fb
)
{
    if (!fb || fb->width < 250 || fb->height < 220)
        return make_rect(0, 0, 0, 0);

    uint64_t x = fb->width >= 650 ? 84 : 16;
    uint64_t y = fb->height >= 360 ? 94 : 50;

    uint64_t right_margin =
        fb->width >= 720 ? 205 : 16;

    uint64_t w = fb->width - x - right_margin;
    uint64_t h = fb->height - y - 46;

    return make_rect(x, y, w, h);
}

gfx_rect_t graphics_close_button_rect(
    struct limine_framebuffer *fb
)
{
    gfx_rect_t c = graphics_console_rect(fb);

    if (c.width < 50)
        return make_rect(0, 0, 0, 0);

    return make_rect(
        c.x + c.width - 23, c.y + 5, 16, 14
    );
}

gfx_rect_t graphics_minimize_button_rect(
    struct limine_framebuffer *fb
)
{
    gfx_rect_t c = graphics_console_rect(fb);

    if (c.width < 50)
        return make_rect(0, 0, 0, 0);

    return make_rect(
        c.x + c.width - 45, c.y + 5, 16, 14
    );
}

gfx_rect_t graphics_taskbar_button_rect(
    struct limine_framebuffer *fb
)
{
    if (!fb || fb->height < 34)
        return make_rect(0, 0, 0, 0);

    return make_rect(
        96, fb->height - 29, 100, 24
    );
}

bool graphics_point_in_rect(
    gfx_rect_t r,
    uint64_t x,
    uint64_t y
)
{
    return r.width != 0 &&
           r.height != 0 &&
           x >= r.x &&
           x - r.x < r.width &&
           y >= r.y &&
           y - r.y < r.height;
}

/* ================= CONSOLE STATE ================= */

void graphics_set_console_state(
    gfx_console_state_t state
)
{
    if (state >= GFX_CONSOLE_OPEN &&
        state <= GFX_CONSOLE_CLOSED) {

        console_state = state;
    }
}

gfx_console_state_t graphics_get_console_state(void)
{
    return console_state;
}

/* ================= CONSOLE WINDOW ================= */

static void draw_console(
    struct limine_framebuffer *fb
)
{
    gfx_rect_t c = graphics_console_rect(fb);

    if (c.width < 50 || c.height < 60)
        return;

    // Window shadow.
    graphics_rect(
        fb, c.x + 4, c.y + 5,
        c.width, c.height, 0x005B477A
    );

    // Outer border.
    graphics_rect(
        fb, c.x, c.y,
        c.width, c.height, COLOR_BORDER
    );

    // Lavender title bar.
    graphics_rect(
        fb, c.x + 2, c.y + 2,
        c.width - 4, 22, 0x00CDA4D7
    );

    // Dark console body.
    graphics_rect(
        fb, c.x + 2, c.y + 24,
        c.width - 4, c.height - 26,
        COLOR_CONSOLE
    );

    graphics_text(
        fb, c.x + 16, c.y + 10,
        "MYOS CONSOLE", 0x00402A52
    );

    gfx_rect_t mini =
        graphics_minimize_button_rect(fb);

    gfx_rect_t close =
        graphics_close_button_rect(fb);

    // Minimize button.
    graphics_rect(
        fb, mini.x, mini.y,
        mini.width, mini.height,
        0x00E9D9F4
    );

    // Close button.
    graphics_rect(
        fb, close.x, close.y,
        close.width, close.height,
        0x00F7C2D0
    );

    graphics_text(
        fb, mini.x + 5, mini.y + 4,
        "_", 0x00402A52
    );

    graphics_text(
        fb, close.x + 5, close.y + 4,
        "X", 0x00402A52
    );
}

/* ================= TASKBAR ================= */

static void draw_taskbar(
    struct limine_framebuffer *fb
)
{
    if (!fb || fb->height < 34)
        return;

    uint64_t y = fb->height - 34;

    graphics_rect(
        fb, 0, y, fb->width, 34, COLOR_BAR
    );

    graphics_rect(
        fb, 0, y, fb->width, 2, 0x00E7CBEF
    );

    // MyOS button.
    graphics_rect(
        fb, 10, y + 5, 78, 24, 0x00DAB8E5
    );

    graphics_text(
        fb, 32, y + 14,
        "MYOS", 0x003C2B4C
    );

    // Console button for future restore.
    gfx_rect_t button =
        graphics_taskbar_button_rect(fb);

    graphics_rect(
        fb,
        button.x, button.y,
        button.width, button.height,
        console_state == GFX_CONSOLE_OPEN
            ? 0x00AE86C8 : 0x00D6C4E5
    );

    graphics_text(
        fb, button.x + 22, button.y + 10,
        "CONSOLE", 0x003B2A50
    );

    if (fb->width > 300) {
        graphics_text(
            fb, fb->width - 77, y + 14,
            "RAM ONLY", 0x00FFF3FB
        );
    }
}

/* ================= DESKTOP RENDERER ================= */

void graphics_draw_desktop(
    struct limine_framebuffer *fb
)
{
    // Base wallpaper.
    graphics_wallpaper(fb);

    // Future layer: RAM file icons.
    // Future layer: anime narrator.
    // Larger narrator when console is hidden.

    if (console_state == GFX_CONSOLE_OPEN) {

        // Task monitor above the console.
        graphics_rect(
            fb, 92, 22, 514, 70, COLOR_BORDER
        );

        graphics_rect(
            fb, 94, 24, 510, 66, COLOR_CONSOLE
        );

        draw_console(fb);
    }

    // Always visible.
    draw_taskbar(fb);

    // Future layer: mouse pointer drawn last.
}

/* ================= CONSOLE CLEAR ================= */

void graphics_clear_console_body(
    struct limine_framebuffer *fb
)
{
    if (console_state != GFX_CONSOLE_OPEN)
        return;

    gfx_rect_t c = graphics_console_rect(fb);

    if (c.width <= 12 || c.height <= 32)
        return;

    graphics_rect(
        fb,
        c.x + 6,
        c.y + 27,
        c.width - 12,
        c.height - 31,
        COLOR_CONSOLE
    );
}
