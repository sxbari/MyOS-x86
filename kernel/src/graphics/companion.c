
#include <stddef.h>
#include "companion.h"
#include "companion_asset.h"
#include "graphics.h"

/* ============== CONFIGURATION ============== */

/* Current MyOS PIT frequency: 100 Hz. */
#define PIT_HZ 100ULL

/* Keep companion above the bottom taskbar. */
#define TASKBAR_PIXELS 42ULL
#define MOTION_PAD 8

/* Margins from screen edges. */
#define COMPANION_MARGIN 6ULL

/* Companion size relative to framebuffer height. */
#define COMPANION_OPEN_PERCENT 74ULL
#define COMPANION_CLOSED_PERCENT 88ULL
#define SWAY_FRAME_TICKS 18ULL
#define BACKUP_MAX_WIDTH 800ULL
#define BACKUP_MAX_HEIGHT 1200ULL
#define COMPOSE_MAX_WIDTH 800ULL
#define COMPOSE_MAX_HEIGHT 1200ULL

static const int8_t sway_wave[16] = {
     0,  1,  2,  3,
     3,  2,  1,  0,
    -1, -2, -3, -3,
    -2, -1,  0,  0
};

/* ============== ANIMATION STATES ============== */

typedef enum {
    POSE_IDLE,
    POSE_TALK,
    POSE_HAPPY,
    POSE_THINK
} pose_t;

/* ============== COMPANION STATE ============== */

static pose_t pose = POSE_IDLE;

static uint64_t now_tick;
static uint64_t last_activity_tick;
static uint64_t reaction_end_tick;

static bool frame_blink;
static bool frame_mouth;
static bool console_is_open;

static uint8_t bob;
static uint8_t effect_frame;

static int8_t head_sway;
static int8_t shoulder_sway;
static int8_t left_sleeve_sway;
static int8_t right_sleeve_sway;

static bool force_redraw;

static uint32_t background[BACKUP_MAX_WIDTH * BACKUP_MAX_HEIGHT];
static uint32_t composed[COMPOSE_MAX_WIDTH * COMPOSE_MAX_HEIGHT];
static uint64_t background_x;
static uint64_t background_y;
static uint64_t background_width;
static uint64_t background_height;
static bool background_valid;
static struct limine_framebuffer *background_fb;

/* ============== RGB565 CONVERSION ============== */

static uint32_t pixel_rgb565(uint16_t p)
{
    /*
     * Sprite image uses RGB565.
     * MyOS framebuffer uses 0x00RRGGBB.
     */

    uint32_t r = (p >> 11) & 31U;
    uint32_t g = (p >> 5) & 63U;
    uint32_t b = p & 31U;

    return (((r * 255U + 15U) / 31U) << 16) |
           (((g * 255U + 31U) / 63U) << 8) |
           ((b * 255U + 15U) / 31U);
}

/* ============== INITIALIZATION ============== */

void companion_init(uint64_t now_ticks)
{
    pose = POSE_IDLE;

    now_tick = now_ticks;
    last_activity_tick = now_ticks;
    reaction_end_tick = now_ticks;

    frame_blink = false;
    frame_mouth = false;

    console_is_open = true;

    bob = 0;
    effect_frame = 0;

    head_sway = 0;
    shoulder_sway = 0;
    left_sleeve_sway = 0;
    right_sleeve_sway = 0;

    force_redraw = true;
    background_valid = false;
    background_fb = NULL;
}

/* ============== CHARACTER REACTIONS ============== */

void companion_react(
    companion_reaction_t reaction,
    uint64_t ticks
)
{
    now_tick = ticks;
    last_activity_tick = ticks;

    force_redraw = true;

    if (reaction == COMPANION_HAPPY) {

        pose = POSE_HAPPY;

        reaction_end_tick =
            ticks + PIT_HZ * 2ULL;

    } else {

        pose = POSE_TALK;

        reaction_end_tick =
            ticks + PIT_HZ * 2ULL;
    }
}

/* ============== ANIMATION UPDATE ============== */

bool companion_update(
    uint64_t ticks,
    bool console_open
)
{
    pose_t old_pose = pose;

    bool old_blink = frame_blink;
    bool old_mouth = frame_mouth;
    bool old_console = console_is_open;

    uint8_t old_bob = bob;
    uint8_t old_effect = effect_frame;
    int8_t old_head_sway = head_sway;
    int8_t old_shoulder_sway = shoulder_sway;
    int8_t old_left_sleeve_sway = left_sleeve_sway;
    int8_t old_right_sleeve_sway = right_sleeve_sway;

    now_tick = ticks;

    console_is_open = console_open;

    /* Return to idle after reactions. */
    if ((pose == POSE_TALK ||
         pose == POSE_HAPPY) &&
        ticks >= reaction_end_tick) {

        pose = POSE_IDLE;
    }

    /* Thinking after 14 seconds of inactivity. */
    if (pose == POSE_IDLE &&
        ticks - last_activity_tick >
            PIT_HZ * 14ULL) {

        pose = POSE_THINK;
    }

    /* Blink approximately every 4.5 seconds. */
    frame_blink =
        (pose == POSE_IDLE ||
         pose == POSE_THINK) &&
        ((ticks % (PIT_HZ * 4ULL + 50ULL)) <
         12ULL);

    /* Talking mouth animation. */
    frame_mouth =
        pose == POSE_TALK &&
        ((ticks / 12ULL) % 2ULL == 0ULL);

    /* Small vertical breathing movement. */
    bob = (uint8_t)(
        (ticks /
        (pose == POSE_HAPPY ? 14ULL : 65ULL))
        % 2ULL
    );

    if (pose == POSE_HAPPY)
        bob *= 2;

    /* Sparkle and thinking animation. */
    effect_frame =
        (uint8_t)((ticks / 24ULL) % 2ULL);

    int8_t wave =
        sway_wave[(ticks / SWAY_FRAME_TICKS) % 16ULL];

    head_sway = (int8_t)(wave * 2);
    shoulder_sway = wave;
    left_sleeve_sway = wave;
    right_sleeve_sway = (int8_t)-wave;

    bool changed =
        force_redraw ||
        old_pose != pose ||
        old_blink != frame_blink ||
        old_mouth != frame_mouth ||
        old_console != console_is_open ||
        old_bob != bob ||
        old_head_sway != head_sway ||
        old_shoulder_sway != shoulder_sway ||
        old_left_sleeve_sway != left_sleeve_sway ||
        old_right_sleeve_sway != right_sleeve_sway ||
        (
            (pose == POSE_HAPPY ||
             pose == POSE_THINK) &&
            old_effect != effect_frame
        );

    force_redraw = false;

    return changed;
}

static int32_t motion_x(int32_t sx, int32_t sy)
{
    int32_t shift;

    if (sy < 145) {
        shift = head_sway;
    }
    else if (sy < 245) {
        shift =
            (head_sway * (245 - sy) +
             shoulder_sway * (sy - 145)) / 100;
    }
    else if (sy < 335) {
        shift = shoulder_sway * (335 - sy) / 90;
    }
    else {
        shift = 0;
    }

    if (sy >= 175 && sy < 445) {
        int32_t strength;

        if (sy < 225)
            strength = (sy - 175) * 2;
        else if (sy < 380)
            strength = 100;
        else
            strength = (445 - sy) * 100 / 65;

        if (sx < 96) {
            shift +=
                left_sleeve_sway *
                (96 - sx) * strength / (96 * 100);
        }
        else if (sx > 200) {
            shift +=
                right_sleeve_sway *
                (sx - 200) * strength / (88 * 100);
        }
    }

    return shift;
}

typedef struct {
    uint64_t x;
    uint64_t y;
    uint64_t width;
    uint64_t height;
    uint64_t max_y;
} companion_layout_t;

static bool companion_get_layout(
    struct limine_framebuffer *fb,
    bool console_open,
    companion_layout_t *layout
)
{
    if (!fb ||
        fb->bpp != 32 ||
        fb->width < 200 ||
        fb->height < 180 ||
        !layout) {
        return false;
    }

    layout->max_y =
        fb->height > TASKBAR_PIXELS
        ? fb->height - TASKBAR_PIXELS
        : fb->height;

    uint64_t usable_h =
        layout->max_y > COMPANION_MARGIN * 2ULL
        ? layout->max_y - COMPANION_MARGIN * 2ULL
        : layout->max_y;

    uint64_t usable_w =
        fb->width > COMPANION_MARGIN * 2ULL
        ? fb->width - COMPANION_MARGIN * 2ULL
        : fb->width;

    uint64_t wanted_h =
        console_open
        ? (fb->height * COMPANION_OPEN_PERCENT) / 100ULL
        : (fb->height * COMPANION_CLOSED_PERCENT) / 100ULL;

    layout->height =
        wanted_h < usable_h ? wanted_h : usable_h;

    layout->width =
        layout->height * COMPANION_ART_WIDTH /
        COMPANION_ART_HEIGHT;

    if (layout->width > usable_w) {
        layout->width = usable_w;
        layout->height =
            layout->width * COMPANION_ART_HEIGHT /
            COMPANION_ART_WIDTH;
    }

    if (!layout->width || !layout->height)
        return false;

    layout->x = fb->width - layout->width;
    if (layout->x >= COMPANION_MARGIN + MOTION_PAD)
        layout->x -= COMPANION_MARGIN + MOTION_PAD;

    layout->y = layout->max_y - layout->height;
    if (layout->y >= COMPANION_MARGIN)
        layout->y -= COMPANION_MARGIN;

    if (layout->y >= bob)
        layout->y -= bob;

    return true;
}

gfx_rect_t companion_bounds(
    struct limine_framebuffer *fb,
    bool console_open
)
{
    companion_layout_t layout;

    if (!companion_get_layout(fb, console_open, &layout))
        return (gfx_rect_t){0};

    uint64_t pad_left =
        layout.x < MOTION_PAD ? layout.x : MOTION_PAD;
    uint64_t pad_right =
        layout.x + layout.width + MOTION_PAD;

    if (pad_right > fb->width)
        pad_right = fb->width;

    return (gfx_rect_t){
        .x = layout.x - pad_left,
        .y = layout.y + bob >= 2 ? layout.y + bob - 2 : 0,
        .width = pad_right - (layout.x - pad_left),
        .height = layout.height + 2
    };
}

void companion_restore_background(struct limine_framebuffer *fb)
{
    if (!background_valid || !fb)
        return;

    if (fb != background_fb || fb->bpp != 32) {
        background_valid = false;
        background_fb = NULL;
        return;
    }

    uint32_t *pixels = (uint32_t *)fb->address;
    uint64_t pitch = fb->pitch / 4ULL;

    for (uint64_t y = 0; y < background_height; y++) {
        for (uint64_t x = 0; x < background_width; x++) {
            pixels[
                (background_y + y) * pitch +
                background_x + x
            ] = background[y * background_width + x];
        }
    }

    background_valid = false;
    background_fb = NULL;
}

/* ============== SPARKLE EFFECT ============== */

static void composed_rect(
    uint64_t x,
    uint64_t y,
    uint64_t width,
    uint64_t height,
    uint32_t color
)
{
    if (x < background_x || y < background_y)
        return;

    uint64_t local_x = x - background_x;
    uint64_t local_y = y - background_y;

    for (uint64_t dy = 0; dy < height; dy++) {
        if (local_y + dy >= background_height)
            break;

        for (uint64_t dx = 0; dx < width; dx++) {
            if (local_x + dx >= background_width)
                break;

            composed[
                (local_y + dy) * background_width +
                local_x + dx
            ] = color;
        }
    }
}

static void composed_sparkle(
    uint64_t x,
    uint64_t y,
    uint32_t color
)
{
    composed_rect(x + 4, y, 2, 13, color);
    composed_rect(x, y + 5, 10, 2, color);
}

/* ============== COMPANION RENDERING ============== */

void companion_draw(
    struct limine_framebuffer *fb
)
{
    companion_layout_t layout;
    if (!companion_get_layout(fb, console_is_open, &layout))
        return;

    /*
     * The companion is allowed to overlap
     * ALL windows.
     *
     * Foreground layering is controlled by
     * drawing her AFTER the console,
     * file viewer, and taskbar in main.c.
     *
     * No console clipping is performed here.
     */

    int64_t start_x = (int64_t)layout.x;
    uint64_t start_y = layout.y;
    uint64_t draw_w = layout.width;
    uint64_t draw_h = layout.height;
    uint64_t max_y = layout.max_y;

    /* ============== FRAMEBUFFER ACCESS ============== */

    uint32_t *pixels =
        (uint32_t *)fb->address;

    uint64_t pitch32 =
        fb->pitch / 4ULL;

    if (background_valid && background_fb != fb) {
        background_valid = false;
        background_fb = NULL;
    }

    uint64_t pad_left =
        layout.x < MOTION_PAD ? layout.x : MOTION_PAD;
    background_x = layout.x - pad_left;

    uint64_t anchored_y = layout.y + bob;
    background_y = anchored_y >= 2 ? anchored_y - 2 : 0;

    background_width = draw_w + pad_left + MOTION_PAD;
    if (background_width > fb->width - background_x)
        background_width = fb->width - background_x;

    background_height = draw_h + 2;
    if (background_height > max_y - background_y)
        background_height = max_y - background_y;

    if (background_width > BACKUP_MAX_WIDTH ||
        background_height > BACKUP_MAX_HEIGHT ||
        background_width > COMPOSE_MAX_WIDTH ||
        background_height > COMPOSE_MAX_HEIGHT ||
        background_height >
            (BACKUP_MAX_WIDTH * BACKUP_MAX_HEIGHT) / background_width) {
        background_valid = false;
        background_fb = NULL;
        return;
    }

    if (!background_valid) {
        for (uint64_t y = 0; y < background_height; y++) {
            for (uint64_t x = 0; x < background_width; x++) {
                background[y * background_width + x] =
                    pixels[
                        (background_y + y) * pitch32 +
                        background_x + x
                    ];
            }
        }

        background_valid = true;
        background_fb = fb;
    }

    for (uint64_t y = 0; y < background_height; y++) {
        for (uint64_t x = 0; x < background_width; x++) {
            composed[y * background_width + x] =
                background[y * background_width + x];
        }
    }

    /* ============== DRAW CHARACTER ============== */

    for (uint64_t dy = 0; dy < draw_h; dy++) {

        int32_t sy = (int32_t)(
            dy * COMPANION_ART_HEIGHT / draw_h
        );

        uint64_t py = start_y + dy;

        /*
         * Protect against screen and taskbar
         * overflow.
         */

        if (py >= max_y)
            break;

        for (int64_t dx = -(int64_t)MOTION_PAD;
             dx < (int64_t)draw_w + MOTION_PAD;
             dx++) {

            int64_t px = start_x + dx;

            if (px < 0 || (uint64_t)px >= fb->width)
                continue;

            /* Source pixel coordinates. */

            int64_t base_sx =
                dx * COMPANION_ART_WIDTH /
                (int64_t)draw_w;
            int64_t sx =
                base_sx - motion_x((int32_t)base_sx, sy);

            if (sx < 0 || sx >= COMPANION_ART_WIDTH)
                continue;

            uint16_t p =
                companion_art[
                    (uint64_t)sy * COMPANION_ART_WIDTH +
                    (uint64_t)sx
                ];

            /* ============== BLINK FRAME ============== */

            if (
                frame_blink &&
                sx >= COMPANION_BLINK_X &&
                sy >= COMPANION_BLINK_Y &&
                sx < COMPANION_BLINK_X +
                     COMPANION_BLINK_WIDTH &&
                sy < COMPANION_BLINK_Y +
                     COMPANION_BLINK_HEIGHT
            ) {

                p = companion_blink_art[
                    ((uint64_t)sy - COMPANION_BLINK_Y) *
                    COMPANION_BLINK_WIDTH +
                    ((uint64_t)sx - COMPANION_BLINK_X)
                ];

            }

            /* ============== TALK FRAME ============== */

            else if (
                frame_mouth &&
                sx >= COMPANION_TALK_X &&
                sy >= COMPANION_TALK_Y &&
                sx < COMPANION_TALK_X +
                     COMPANION_TALK_WIDTH &&
                sy < COMPANION_TALK_Y +
                     COMPANION_TALK_HEIGHT
            ) {

                p = companion_talk_art[
                    ((uint64_t)sy - COMPANION_TALK_Y) *
                    COMPANION_TALK_WIDTH +
                    ((uint64_t)sx - COMPANION_TALK_X)
                ];
            }

            /* ============== WRITE PIXEL ============== */

            /*
             * Zero represents transparency.
             *
             * All nontransparent sprite pixels
             * are drawn regardless of whether
             * the console or viewer is underneath.
             */

            if (p != 0) {

                composed[
                    (py - background_y) * background_width +
                    ((uint64_t)px - background_x)
                ] = pixel_rgb565(p);
            }
        }
    }

    /* ============== REACTION EFFECTS ============== */

    uint64_t e_x =
        start_x + draw_w - 24;

    uint64_t e_y =
        start_y + draw_h / 6;

    /*
     * Only draw effects when enough room
     * remains inside the screen.
     */

    if (e_x < fb->width &&
        fb->width - e_x >= 24 &&
        e_y < max_y &&
        max_y - e_y >= 44) {

        /* ============== HAPPY EFFECT ============== */

        if (pose == POSE_HAPPY) {

            composed_sparkle(
                e_x,
                e_y,
                effect_frame
                ? 0x00FFE6A4U
                : 0x00FFB9DEU
            );

            composed_sparkle(
                e_x + 4,
                e_y + 28,
                effect_frame
                ? 0x00FFB9DEU
                : 0x00FFE6A4U
            );
        }

        /* ============== THINKING EFFECT ============== */

        else if (pose == POSE_THINK) {

            composed_rect(
                e_x, e_y,
                20, 14,
                0x00F1D8FCU
            );

            composed_rect(
                e_x + 2, e_y + 2,
                16, 10,
                0x005B3C79U
            );

            composed_rect(
                e_x + 5, e_y + 7,
                2, 2,
                0x00FFFFFFU
            );

            composed_rect(
                e_x + 9, e_y + 7,
                2, 2,
                0x00FFFFFFU
            );

            composed_rect(
                e_x + 13, e_y + 7,
                2, 2,
                0x00FFFFFFU
            );
        }
    }

    for (uint64_t y = 0; y < background_height; y++) {
        for (uint64_t x = 0; x < background_width; x++) {
            pixels[
                (background_y + y) * pitch32 +
                background_x + x
            ] = composed[y * background_width + x];
        }
    }
}
