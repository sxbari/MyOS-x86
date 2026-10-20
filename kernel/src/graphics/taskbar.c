
#include "taskbar.h"
#include "graphics.h"
#include "../filesystem/ramfs.h"
#include <stddef.h>

/* ============== COLORS ============== */

#define TB_BASE      0x008B68B4U
#define TB_EDGE      0x005B397EU
#define TB_BUTTON    0x00DCC6F2U
#define TB_ACTIVE    0x00F8DAA5U
#define TB_TEXT      0x00392159U
#define TB_MENU_BG   0x00E9D5F5U
#define TB_ROW       0x00FAF1FFU
#define TB_CLOCK_BG  0x006A488EU
#define TB_CLOCK_FG  0x00FDF5FFU

/* ============== DIMENSIONS ============== */

#define TB_H 42U
#define TB_BTN_H 27U
#define TB_MENU_W 216U
#define TB_MENU_ITEM_H 28U
#define TB_FILES_MAX 6U

typedef struct {
    uint64_t x, y, w, h;
} rect_t;

typedef enum {
    MENU_NONE,
    MENU_MAIN,
    MENU_FILES
} menu_t;

static menu_t menu_mode;
static bool invalidated;
static uint64_t last_uptime;

/* ============== GEOMETRY ============== */

static bool inside(rect_t r, uint64_t x, uint64_t y)
{
    return x >= r.x &&
           y >= r.y &&
           x - r.x < r.w &&
           y - r.y < r.h;
}

static rect_t bar_rect(struct limine_framebuffer *fb)
{
    rect_t r = {0, 0, 0, 0};

    if (!fb)
        return r;

    r.w = fb->width;
    r.h = fb->height < TB_H ? fb->height : TB_H;
    r.y = fb->height - r.h;

    return r;
}

static rect_t button_rect(
    struct limine_framebuffer *fb,
    unsigned number
)
{
    rect_t bar = bar_rect(fb);

    rect_t r = {
        10 + (uint64_t)number * 117,
        bar.y + 7,
        108,
        TB_BTN_H
    };

    return r;
}

static rect_t menu_rect(struct limine_framebuffer *fb)
{
    rect_t bar = bar_rect(fb);

    uint64_t h =
        (menu_mode == MENU_FILES ?
         TB_FILES_MAX + 2 : 6) *
        TB_MENU_ITEM_H + 10;

    if (h > bar.y)
        h = bar.y;

    rect_t r = {
        10,
        bar.y - h,
        TB_MENU_W,
        h
    };

    return r;
}

/* ============== DRAW HELPERS ============== */

static void fill(
    struct limine_framebuffer *fb,
    rect_t r,
    uint32_t color
)
{
    if (!fb ||
        r.x >= fb->width ||
        r.y >= fb->height)
        return;

    if (r.w > fb->width - r.x)
        r.w = fb->width - r.x;

    if (r.h > fb->height - r.y)
        r.h = fb->height - r.y;

    graphics_rect(
        fb, r.x, r.y,
        r.w, r.h, color
    );
}

static void label(
    struct limine_framebuffer *fb,
    uint64_t x,
    uint64_t y,
    const char *s,
    uint32_t color
)
{
    graphics_text(fb, x, y, s, color);
}

static void button(
    struct limine_framebuffer *fb,
    rect_t r,
    const char *title,
    bool active
)
{
    fill(fb, r, TB_EDGE);

    rect_t inner = {
        r.x + 2,
        r.y + 2,
        r.w - 4,
        r.h - 4
    };

    fill(
        fb, inner,
        active ? TB_ACTIVE : TB_BUTTON
    );

    label(
        fb,
        r.x + 8,
        r.y + 9,
        title,
        TB_TEXT
    );
}

/* ============== RAMFS FILE LIST ============== */

static uint32_t file_count(void)
{
    uint32_t n = 0;

    for (size_t i = 0; i < RAMFS_MAX_FILES; i++) {
        if (ramfs_name(i))
            n++;
    }

    return n;
}

static const char *file_at(
    uint32_t ordinal,
    size_t *slot
)
{
    uint32_t n = 0;

    for (size_t i = 0; i < RAMFS_MAX_FILES; i++) {

        const char *name = ramfs_name(i);

        if (!name)
            continue;

        if (n++ == ordinal) {

            if (slot)
                *slot = i;

            return name;
        }
    }

    return NULL;
}

/* ============== LAUNCHER MENU ============== */

static void draw_menu(
    struct limine_framebuffer *fb
)
{
    if (menu_mode == MENU_NONE)
        return;

    rect_t m = menu_rect(fb);

    if (m.h < 45)
        return;

    rect_t shadow = {
        m.x + 3, m.y + 3,
        m.w, m.h
    };

    fill(fb, shadow, TB_EDGE);
    fill(fb, m, TB_MENU_BG);

    rect_t header = {
        m.x + 3, m.y + 3,
        m.w - 6, TB_MENU_ITEM_H
    };

    fill(fb, header, TB_BASE);

    label(
        fb, m.x + 12, m.y + 13,
        menu_mode == MENU_MAIN ?
            "MYOS MENU" : "MY FILES",
        TB_CLOCK_FG
    );

    if (menu_mode == MENU_MAIN) {

        static const char *names[5] = {
            "CONSOLE",
            "FILES",
            "SHOW DESKTOP",
            "ABOUT MYOS",
            "REBOOT"
        };

        for (unsigned i = 0; i < 5; i++) {

            rect_t row = {
                m.x + 4,
                m.y + 4 +
                    (uint64_t)(i + 1) * TB_MENU_ITEM_H,
                m.w - 8,
                TB_MENU_ITEM_H - 2
            };

            if (row.y + row.h > m.y + m.h)
                break;

            fill(fb, row, TB_ROW);

            label(
                fb,
                row.x + 10,
                row.y + 9,
                names[i],
                TB_TEXT
            );
        }
    }
    else {

        uint32_t n = file_count();

        if (!n) {
            label(
                fb,
                m.x + 14,
                m.y + TB_MENU_ITEM_H + 15,
                "NO FILES",
                TB_TEXT
            );
        }

        uint32_t shown =
            n < TB_FILES_MAX ? n : TB_FILES_MAX;

        for (uint32_t i = 0; i < shown; i++) {

            rect_t row = {
                m.x + 4,
                m.y + 4 +
                    (uint64_t)(i + 1) * TB_MENU_ITEM_H,
                m.w - 8,
                TB_MENU_ITEM_H - 2
            };

            fill(fb, row, TB_ROW);

            const char *name = file_at(i, NULL);

            char visible[25];
            size_t j = 0;

            while (name && name[j] && j < 24) {
                visible[j] = name[j];
                j++;
            }

            visible[j] = '\0';

            label(
                fb,
                row.x + 10,
                row.y + 9,
                visible,
                TB_TEXT
            );
        }

        rect_t back = {
            m.x + 4,
            m.y + 4 + 7U * TB_MENU_ITEM_H,
            m.w - 8,
            TB_MENU_ITEM_H - 2
        };

        if (back.y + back.h <= m.y + m.h) {

            fill(fb, back, TB_ROW);

            label(
                fb,
                back.x + 10,
                back.y + 9,
                "BACK",
                TB_TEXT
            );
        }
    }
}

/* ============== NUMBER CONVERSION ============== */

static void u64_to_str(uint64_t n, char *out)
{
    char temp[21];
    unsigned i = 0;
    unsigned j = 0;

    do {
        temp[i++] = (char)('0' + n % 10);
        n /= 10;
    } while (n);

    while (i)
        out[j++] = temp[--i];

    out[j] = '\0';
}

/* ============== TASKBAR STATE ============== */

void taskbar_init(void)
{
    menu_mode = MENU_NONE;
    invalidated = true;
    last_uptime = UINT64_MAX;
}

void taskbar_invalidate(void)
{
    invalidated = true;
}

bool taskbar_needs_draw(uint64_t uptime_seconds)
{
    return invalidated ||
           last_uptime != uptime_seconds;
}

/* ============== TASKBAR DRAWING ============== */

void taskbar_draw(
    struct limine_framebuffer *fb,
    uint64_t uptime_seconds,
    bool console_open,
    bool viewer_open,
    bool viewer_front
)
{
    if (!fb || fb->bpp != 32)
        return;

    rect_t bar = bar_rect(fb);

    fill(fb, bar, TB_EDGE);

    rect_t interior = {
        0,
        bar.y + 3,
        bar.w,
        bar.h > 3 ? bar.h - 3 : 0
    };

    fill(fb, interior, TB_BASE);

    // MyOS launcher
    button(
        fb,
        button_rect(fb, 0),
        "MYOS",
        menu_mode != MENU_NONE
    );

    // Console taskbar button
    button(
        fb,
        button_rect(fb, 1),
        "CONSOLE",
        console_open && !viewer_front
    );

    // File viewer appears only when a file is open.
    if (viewer_open) {
        button(
            fb,
            button_rect(fb, 2),
            "VIEWER",
            viewer_front
        );
    }

    /* ============== UPTIME CLOCK ============== */

    if (fb->width > 465) {

        rect_t clock = {
            fb->width - 115,
            bar.y + 8,
            105,
            25
        };

        fill(fb, clock, TB_CLOCK_BG);

        char digits[21];
        char display[30];

        unsigned p = 0;

        display[p++] = 'U';
        display[p++] = 'P';
        display[p++] = ' ';

        // Minutes
        u64_to_str(uptime_seconds / 60, digits);

        for (unsigned i = 0;
             digits[i] && p < 25;
             i++) {

            display[p++] = digits[i];
        }

        display[p++] = ':';

        // Seconds
        uint64_t seconds = uptime_seconds % 60;

        display[p++] =
            (char)('0' + seconds / 10);

        display[p++] =
            (char)('0' + seconds % 10);

        display[p] = '\0';

        label(
            fb,
            clock.x + 7,
            clock.y + 9,
            display,
            TB_CLOCK_FG
        );
    }

    // Launcher menu is drawn over the desktop.
    draw_menu(fb);

    invalidated = false;
    last_uptime = uptime_seconds;
}

/* ============== TASKBAR CLICK HANDLING ============== */

taskbar_action_t taskbar_click(
    struct limine_framebuffer *fb,
    uint64_t x,
    uint64_t y,
    bool viewer_open,
    size_t *file_slot
)
{
    if (!fb)
        return TASKBAR_NONE;

    rect_t bar = bar_rect(fb);

    /* ============== TASKBAR BUTTONS ============== */

    if (inside(bar, x, y)) {

        if (inside(button_rect(fb, 0), x, y)) {

            menu_mode =
                menu_mode == MENU_NONE ?
                    MENU_MAIN : MENU_NONE;

            invalidated = true;
            return TASKBAR_REDRAW;
        }

        if (inside(button_rect(fb, 1), x, y)) {

            menu_mode = MENU_NONE;
            invalidated = true;

            return TASKBAR_CONSOLE;
        }

        if (viewer_open &&
            inside(button_rect(fb, 2), x, y)) {

            menu_mode = MENU_NONE;
            invalidated = true;

            return TASKBAR_VIEWER;
        }

        return TASKBAR_REDRAW;
    }

    /* ============== LAUNCHER MENU CLICKS ============== */

    if (menu_mode != MENU_NONE) {

        rect_t m = menu_rect(fb);

        if (inside(m, x, y)) {

            if (y < m.y + TB_MENU_ITEM_H + 4)
                return TASKBAR_REDRAW;

            uint64_t rel =
                (y - (m.y + 4)) / TB_MENU_ITEM_H;

            if (rel == 0)
                return TASKBAR_REDRAW;

            uint32_t item = (uint32_t)(rel - 1);

            if (menu_mode == MENU_MAIN) {

                // FILES opens the RAMFS file list.
                if (item == 1) {

                    menu_mode = MENU_FILES;
                    invalidated = true;

                    return TASKBAR_REDRAW;
                }

                menu_mode = MENU_NONE;
                invalidated = true;

                switch (item) {
                    case 0: return TASKBAR_CONSOLE;
                    case 2: return TASKBAR_DESKTOP;
                    case 3: return TASKBAR_ABOUT;
                    case 4: return TASKBAR_REBOOT;
                    default: return TASKBAR_REDRAW;
                }
            }

            /* ============== FILE LIST ============== */

            if (item < TB_FILES_MAX &&
                item < file_count()) {

                if (file_at(item, file_slot)) {

                    menu_mode = MENU_NONE;
                    invalidated = true;

                    return TASKBAR_FILE;
                }
            }

            // BACK returns to the launcher.
            if (item == 6) {

                menu_mode = MENU_MAIN;
                invalidated = true;
            }

            return TASKBAR_REDRAW;
        }

        // Clicking outside dismisses the launcher.
        menu_mode = MENU_NONE;
        invalidated = true;

        return TASKBAR_REDRAW;
    }

    return TASKBAR_NONE;
}
