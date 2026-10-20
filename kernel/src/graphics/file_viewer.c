
#include "file_viewer.h"
#include "graphics.h"
#include "../filesystem/ramfs.h"

#define VIEWER_NAME_CAP 128
#define VIEWER_GLYPH_SCALE 2
#define VIEWER_GLYPH_STEP 12
#define VIEWER_LINE_STEP 19
#define VIEWER_CONTENT_SCAN_LIMIT 16384

/* ============== PASTEL COLORS ============== */

#define V_BORDER  0x006A468D
#define V_SHADOW  0x0066477A
#define V_FRAME   0x00E2C7EE
#define V_BAR     0x00B691D6
#define V_BODY    0x00352250
#define V_TEXT    0x00FDF4FF
#define V_ACCENT  0x00FFE7AA
#define V_DIM     0x00CBB4D9
#define V_BTN     0x00F1DFF7

/* ============== VIEWER STATE ============== */

static bool opened;
static bool dirty;

static char current_name[VIEWER_NAME_CAP];
static uint32_t scroll_line;

static bool same_name(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a++ != *b++)
            return false;
    }

    return *a == '\0' && *b == '\0';
}

void file_viewer_init(void)
{
    opened = false;
    dirty = false;
    scroll_line = 0;
    current_name[0] = '\0';
}

bool file_viewer_open(const char *name)
{
    if (!name || !*name || !ramfs_read(name))
        return false;

    uint32_t n = 0;

    while (name[n] && n + 1 < VIEWER_NAME_CAP)
        n++;

    if (name[n])
        return false;

    for (uint32_t i = 0; i < n; i++)
        current_name[i] = name[i];

    current_name[n] = '\0';

    scroll_line = 0;
    opened = true;
    dirty = true;

    return true;
}

void file_viewer_close(void)
{
    opened = false;
    dirty = false;
}

bool file_viewer_is_open(void)
{
    return opened;
}

bool file_viewer_needs_redraw(void)
{
    return opened && dirty;
}

void file_viewer_mark_dirty(void)
{
    if (opened)
        dirty = true;
}

bool file_viewer_displays(const char *name)
{
    return opened &&
           name &&
           same_name(current_name, name);
}

/* ============== WINDOW GEOMETRY ============== */

gfx_rect_t file_viewer_bounds(
    struct limine_framebuffer *fb
)
{
    gfx_rect_t r = {0, 0, 0, 0};

    if (!fb || fb->width < 240 || fb->height < 230)
        return r;

    r.width =
        fb->width > 520 ? 480 : fb->width - 40;

    r.height =
        fb->height > 440 ? 320 : fb->height - 96;

    r.x = (fb->width - r.width) / 2;

    uint64_t available_height = fb->height - 42;

    r.y = (available_height - r.height) / 2;

    return r;
}

static gfx_rect_t close_rect(gfx_rect_t r)
{
    gfx_rect_t b = {
        r.x + r.width - 33,
        r.y + 6,
        24,
        21
    };

    return b;
}

static gfx_rect_t up_rect(gfx_rect_t r)
{
    gfx_rect_t b = {
        r.x + r.width - 70,
        r.y + r.height - 29,
        26,
        20
    };

    return b;
}

static gfx_rect_t down_rect(gfx_rect_t r)
{
    gfx_rect_t b = {
        r.x + r.width - 38,
        r.y + r.height - 29,
        26,
        20
    };

    return b;
}

/* ============== CLICK HANDLING ============== */

bool file_viewer_handle_click(
    struct limine_framebuffer *fb,
    uint64_t x,
    uint64_t y
)
{
    if (!opened)
        return false;

    gfx_rect_t r = file_viewer_bounds(fb);

    if (!r.width ||
        !graphics_point_in_rect(r, x, y))
        return false;

    if (graphics_point_in_rect(close_rect(r), x, y)) {

        file_viewer_close();

    } else if (graphics_point_in_rect(up_rect(r), x, y)) {

        if (scroll_line > 0)
            scroll_line--;

        dirty = true;

    } else if (graphics_point_in_rect(down_rect(r), x, y)) {

        if (scroll_line < UINT32_MAX)
            scroll_line++;

        dirty = true;
    }

    // A click inside the viewer must not pass
    // through to the console or desktop icons.
    return true;
}

/* ============== PIXEL FONT ============== */

static const uint8_t glyphs[36][7] = {
    {14,17,17,31,17,17,17},
    {30,17,17,30,17,17,30},
    {15,16,16,16,16,16,15},
    {30,17,17,17,17,17,30},
    {31,16,16,30,16,16,16},
    {31,16,16,30,16,16,16},
    {15,16,16,23,17,17,15},
    {17,17,17,31,17,17,17},
    {31,4,4,4,4,4,31},
    {7,2,2,2,18,18,12},
    {17,18,20,24,20,18,17},
    {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17},
    {17,25,21,19,17,17,17},
    {14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16},
    {14,17,17,17,21,18,13},
    {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30},
    {31,4,4,4,4,4,4},
    {17,17,17,17,17,17,14},
    {17,17,17,17,17,10,4},
    {17,17,17,21,21,21,10},
    {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4},
    {31,1,2,4,8,16,31},

    {14,17,19,21,25,17,14},
    {4,12,4,4,4,4,14},
    {14,17,1,2,4,8,31},
    {30,1,1,6,1,17,14},
    {2,6,10,18,31,2,2},
    {31,16,30,1,1,17,14},
    {14,16,30,17,17,17,14},
    {31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14}
};

static uint8_t glyph_row(char c, int row)
{
    if (c >= 'a' && c <= 'z')
        c -= 'a' - 'A';

    if (c >= 'A' && c <= 'Z')
        return glyphs[c - 'A'][row];

    if (c >= '0' && c <= '9')
        return glyphs[26 + c - '0'][row];

    if (c == '.')
        return row == 6 ? 4 : 0;

    if (c == '-')
        return row == 3 ? 14 : 0;

    if (c == '_')
        return row == 6 ? 31 : 0;

    if (c == '/')
        return (uint8_t)(
            1U << (row < 5 ? row : 4)
        );

    if (c == ':')
        return row == 2 || row == 5 ? 4 : 0;

    if (c == '=')
        return row == 2 || row == 4 ? 14 : 0;

    if (c == '!')
        return row < 5 || row == 6 ? 4 : 0;

    if (c == '?') {
        static const uint8_t q[7] =
            {14,17,1,2,4,0,4};

        return q[row];
    }

    return 0;
}

/* ============== DRAW CHARACTER ============== */

static void draw_char(
    struct limine_framebuffer *fb,
    uint64_t x,
    uint64_t y,
    char c,
    uint32_t color,
    uint64_t clip_right,
    uint64_t clip_bottom
)
{
    for (int row = 0; row < 7; row++) {

        uint8_t bits = glyph_row(c, row);

        for (int col = 0; col < 5; col++) {

            if (!(bits & (1U << (4 - col))))
                continue;

            for (int yy = 0; yy < VIEWER_GLYPH_SCALE; yy++) {

                for (int xx = 0; xx < VIEWER_GLYPH_SCALE; xx++) {

                    uint64_t px =
                        x + (uint64_t)col * 2 + xx;

                    uint64_t py =
                        y + (uint64_t)row * 2 + yy;

                    if (px < clip_right &&
                        py < clip_bottom) {

                        graphics_pixel(
                            fb, px, py, color
                        );
                    }
                }
            }
        }
    }
}

/* ============== DRAW TEXT ============== */

static void draw_text(
    struct limine_framebuffer *fb,
    uint64_t x,
    uint64_t y,
    const char *text,
    uint32_t color,
    uint64_t right,
    uint64_t bottom
)
{
    while (*text &&
           x + VIEWER_GLYPH_STEP <= right) {

        draw_char(
            fb, x, y, *text,
            color, right, bottom
        );

        text++;
        x += VIEWER_GLYPH_STEP;
    }
}

/* ============== LINE COUNTING ============== */

static uint32_t count_lines(
    const char *contents,
    uint32_t cols
)
{
    uint32_t line = 0;
    uint32_t col = 0;

    if (!contents || !*contents || cols == 0)
        return 1;

    for (uint32_t i = 0;
         i < VIEWER_CONTENT_SCAN_LIMIT && contents[i];
         i++) {

        char c = contents[i];

        if (c == '\r')
            continue;

        if (c == '\n') {
            line++;
            col = 0;
            continue;
        }

        if (col >= cols) {
            line++;
            col = 0;
        }

        col++;
    }

    return line + 1;
}

/* ============== CONTENT DRAWING ============== */

static void draw_contents(
    struct limine_framebuffer *fb,
    gfx_rect_t r,
    const char *contents
)
{
    uint64_t left = r.x + 21;
    uint64_t top = r.y + 67;
    uint64_t right = r.x + r.width - 18;
    uint64_t bottom = r.y + r.height - 41;

    if (right <= left || bottom <= top)
        return;

    uint32_t cols =
        (uint32_t)((right - left) / VIEWER_GLYPH_STEP);

    uint32_t rows =
        (uint32_t)((bottom - top) / VIEWER_LINE_STEP);

    if (!cols || !rows)
        return;

    if (!contents || !*contents) {
        draw_text(
            fb, left, top,
            "EMPTY FILE",
            V_DIM, right, bottom
        );
        return;
    }

    uint32_t total_lines =
        count_lines(contents, cols);

    uint32_t max_scroll =
        total_lines > rows ? total_lines - rows : 0;

    if (scroll_line > max_scroll)
        scroll_line = max_scroll;

    uint32_t line = 0;
    uint32_t col = 0;

    for (uint32_t i = 0;
         i < VIEWER_CONTENT_SCAN_LIMIT && contents[i];
         i++) {

        char c = contents[i];

        if (c == '\r')
            continue;

        if (c == '\n') {
            line++;
            col = 0;
            continue;
        }

        if (c == '\t')
            c = ' ';

        if (col >= cols) {
            line++;
            col = 0;
        }

        if (line >= scroll_line &&
            line < scroll_line + rows) {

            draw_char(
                fb,
                left + (uint64_t)col * VIEWER_GLYPH_STEP,
                top + (uint64_t)(line - scroll_line) *
                      VIEWER_LINE_STEP,
                c,
                V_TEXT,
                right,
                bottom
            );
        }

        col++;
    }
}

/* ============== DRAW VIEWER WINDOW ============== */

void file_viewer_draw(
    struct limine_framebuffer *fb
)
{
    if (!opened || !fb || fb->bpp != 32)
        return;

    gfx_rect_t r = file_viewer_bounds(fb);

    if (!r.width)
        return;

    // Window shadow
    graphics_rect(
        fb, r.x + 5, r.y + 6,
        r.width, r.height,
        V_SHADOW
    );

    // Outer border
    graphics_rect(
        fb, r.x, r.y,
        r.width, r.height,
        V_BORDER
    );

    // Pastel window frame
    graphics_rect(
        fb, r.x + 3, r.y + 3,
        r.width - 6, r.height - 6,
        V_FRAME
    );

    // Title bar
    graphics_rect(
        fb, r.x + 5, r.y + 5,
        r.width - 10, 28,
        V_BAR
    );

    // Dark content area
    graphics_rect(
        fb, r.x + 8, r.y + 35,
        r.width - 16, r.height - 45,
        V_BODY
    );

    // Close button
    gfx_rect_t close = close_rect(r);

    graphics_rect(
        fb,
        close.x, close.y,
        close.width, close.height,
        V_BTN
    );

    draw_text(
        fb, close.x + 7, close.y + 3,
        "X", V_BORDER,
        close.x + close.width,
        close.y + close.height
    );

    // Filename in title bar
    draw_text(
        fb, r.x + 17, r.y + 11,
        current_name,
        V_BORDER,
        close.x - 6,
        r.y + 32
    );

    // Content heading
    draw_text(
        fb, r.x + 21, r.y + 44,
        "FILE CONTENTS",
        V_ACCENT,
        r.x + r.width - 20,
        r.y + 65
    );

    // Read real data from RAMFS
    const char *contents =
        ramfs_read(current_name);

    draw_contents(fb, r, contents);

    // Scrolling controls
    gfx_rect_t up = up_rect(r);
    gfx_rect_t down = down_rect(r);

    graphics_rect(
        fb, up.x, up.y,
        up.width, up.height,
        V_BTN
    );

    graphics_rect(
        fb, down.x, down.y,
        down.width, down.height,
        V_BTN
    );

    draw_text(
        fb, up.x + 8, up.y + 2,
        "U", V_BORDER,
        up.x + up.width,
        up.y + up.height
    );

    draw_text(
        fb, down.x + 8, down.y + 2,
        "D", V_BORDER,
        down.x + down.width,
        down.y + down.height
    );

    draw_text(
        fb,
        r.x + 17,
        r.y + r.height - 26,
        "RAMFS READ ONLY",
        V_DIM,
        up.x - 4,
        r.y + r.height - 6
    );

    dirty = false;
}
