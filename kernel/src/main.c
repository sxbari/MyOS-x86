
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limine.h>

#include "memory/heap.h"
#include "filesystem/ramfs.h"
#include "interrupts/timer.h"
#include "scheduler/scheduler.h"
#include "graphics/graphics.h"
#include "interrupts/mouse.h"
#include "graphics/mouse_cursor.h"
#include "graphics/desktop_files.h"
#include "graphics/file_viewer.h"
#include "graphics/taskbar.h"
#include "graphics/companion.h"

/* ================= CONSOLE STYLE ================= */

#define COLOR_NORMAL  0x00F7F0FF
#define COLOR_COMMAND 0x009CFFBB
#define COLOR_HEADING 0x00FFE39A
#define COLOR_SUCCESS 0x0098F3CC
#define COLOR_ERROR   0x00FF9FB5
#define COLOR_PROMPT  0x00C7B5FF
#define COLOR_MUTED   0x00C4C1DD
#define COLOR_BG      0x0030104F

#define FONT_SCALE 2
#define CHAR_STEP (6 * FONT_SCALE)
#define LINE_STEP 22

#define CONSOLE_MAX_COLS 120
#define CONSOLE_MAX_ROWS 32
#define INPUT_QUEUE_SIZE 256
#define POWER_INTERVAL_TICKS 50

void idt_init(void);
void pic_remap(uint64_t hhdm_offset);
void pic_enable_mouse_irq(void);

static bool gui_redraw_needed = false;
static bool viewer_minimized = false;

/* ================= LIMINE REQUESTS ================= */

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile uint64_t base_revision[] =
    LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests")))
volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t requests_start[] =
    LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t requests_end[] =
    LIMINE_REQUESTS_END_MARKER;

/* ================= KERNEL HALT ================= */

static void hcf(void)
{
    for (;;)
        asm volatile ("hlt");
}

/* ================= LARGE CONSOLE FONT ================= */

/*
 * 5x7 pixel font, drawn at scale 2.
 *
 * Actual glyph size: 10x14 pixels
 * Character advance: 12 pixels
 * Line spacing: 22 pixels
 */

static const uint8_t font[36][7] = {
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

/* ================= SYMBOL GLYPHS ================= */

static uint8_t glyph_row(char ch, int row)
{
    if (ch >= 'a' && ch <= 'z')
        ch -= 'a' - 'A';

    if (ch >= 'A' && ch <= 'Z')
        return font[ch - 'A'][row];

    if (ch >= '0' && ch <= '9')
        return font[26 + ch - '0'][row];

    switch (ch) {
        case '.':
            return row == 6 ? 4 : 0;

        case ',':
            return row == 5 ? 4 :
                   row == 6 ? 8 : 0;

        case ':':
            return row == 2 || row == 5 ? 4 : 0;

        case ';':
            return row == 2 ? 4 :
                   row == 5 ? 4 :
                   row == 6 ? 8 : 0;

        case '-':
            return row == 3 ? 14 : 0;

        case '_':
            return row == 6 ? 31 : 0;

        case '>':
            return row == 1 || row == 5 ? 8 :
                   row == 2 || row == 4 ? 4 :
                   row == 3 ? 2 : 0;

        case '<':
            return row == 1 || row == 5 ? 2 :
                   row == 2 || row == 4 ? 4 :
                   row == 3 ? 8 : 0;

        case '/':
            return row == 0 ? 1 :
                   row == 1 ? 2 :
                   row == 2 ? 2 :
                   row == 3 ? 4 :
                   row == 4 ? 8 :
                   row == 5 ? 8 : 16;

        case '\\':
            return row == 0 ? 16 :
                   row == 1 ? 8 :
                   row == 2 ? 8 :
                   row == 3 ? 4 :
                   row == 4 ? 2 :
                   row == 5 ? 2 : 1;

        case '=':
            return row == 2 || row == 4 ? 14 : 0;

        case '+':
            return row == 3 ? 31 :
                   row == 1 || row == 2 ||
                   row == 4 || row == 5 ? 4 : 0;

        case '!':
            return row <= 4 || row == 6 ? 4 : 0;

        case '?': {
            static const uint8_t p[7] =
                {14,17,1,2,4,0,4};

            return p[row];
        }

        case '(':
            return row == 0 || row == 6 ? 2 : 4;

        case ')':
            return row == 0 || row == 6 ? 8 : 4;

        case '^':
            return row == 0 ? 4 :
                   row == 1 ? 10 :
                   row == 2 ? 17 : 0;

        case '#':
            return row == 2 || row == 4 ? 31 : 10;

        case '*':
            return row == 2 ? 21 :
                   row == 3 ? 14 :
                   row == 4 ? 21 : 0;

        default:
            return 0;
    }
}

/* ================= SCALED DRAWING ================= */

static void draw_scaled_char(
    struct limine_framebuffer *fb,
    uint64_t x,
    uint64_t y,
    char ch,
    uint32_t color,
    uint64_t max_x,
    uint64_t max_y
)
{
    for (uint64_t r = 0; r < 7; r++) {
        uint8_t bits = glyph_row(ch, (int)r);

        for (uint64_t c = 0; c < 5; c++) {
            if (!(bits & (1U << (4 - c))))
                continue;

            for (uint64_t yy = 0;
                 yy < FONT_SCALE; yy++) {

                for (uint64_t xx = 0;
                     xx < FONT_SCALE; xx++) {

                    uint64_t px =
                        x + c * FONT_SCALE + xx;

                    uint64_t py =
                        y + r * FONT_SCALE + yy;

                    if (px < max_x && py < max_y) {
                        graphics_pixel(
                            fb, px, py, color
                        );
                    }
                }
            }
        }
    }
}

static uint64_t draw_label(
    struct limine_framebuffer *fb,
    uint64_t x,
    uint64_t y,
    const char *text,
    uint32_t color,
    uint64_t max_x,
    uint64_t max_y
)
{
    while (*text && x + CHAR_STEP <= max_x) {
        draw_scaled_char(
            fb, x, y, *text++,
            color, max_x, max_y
        );

        x += CHAR_STEP;
    }

    return x;
}

/* ================= STRING UTILITIES ================= */

static void uint_to_string(
    uint64_t value,
    char out[21]
)
{
    char temp[21];
    int n = 0;

    do {
        temp[n++] = '0' + value % 10;
        value /= 10;
    } while (value);

    int i = 0;

    while (n)
        out[i++] = temp[--n];

    out[i] = '\0';
}

static bool string_equals(
    const char *a,
    const char *b
)
{
    while (*a && *b) {
        if (*a++ != *b++)
            return false;
    }

    return *a == '\0' && *b == '\0';
}

static bool starts_with(
    const char *text,
    const char *prefix
)
{
    while (*prefix) {
        if (*text++ != *prefix++)
            return false;
    }

    return true;
}

static void console_line(
    const char *text,
    uint32_t color
);

static void print_value_line(
    const char *label,
    uint64_t value
)
{
    char buffer[21];

    uint_to_string(value, buffer);

    console_line(label, COLOR_HEADING);
    console_line(buffer, COLOR_NORMAL);
}

static void print_result(
    bool ok,
    const char *success,
    const char *failure
)
{
    console_line(
        ok ? success : failure,
        ok ? COLOR_SUCCESS : COLOR_ERROR
    );
}

/* ================= CPU INFORMATION ================= */

static void get_cpu_vendor(char vendor[13])
{
    uint32_t a, b, c, d;

    asm volatile (
        "cpuid"
        : "=a"(a), "=b"(b),
          "=c"(c), "=d"(d)
        : "a"(0)
    );

    (void)a;

    for (int i = 0; i < 4; i++) {
        vendor[i]     = (char)(b >> (i * 8));
        vendor[i + 4] = (char)(d >> (i * 8));
        vendor[i + 8] = (char)(c >> (i * 8));
    }

    vendor[12] = '\0';
}

/* ================= CONSOLE STORAGE ================= */

/*
 * Each visible character is stored together
 * with its color.
 *
 * This allows:
 * - Large text
 * - Colored output
 * - Automatic wrapping
 * - Scrolling
 * - Screen redraws
 */

static char cells[CONSOLE_MAX_ROWS][CONSOLE_MAX_COLS];
static char rendered_cells[CONSOLE_MAX_ROWS][CONSOLE_MAX_COLS];

static uint32_t
    cell_colors[CONSOLE_MAX_ROWS][CONSOLE_MAX_COLS];
static uint32_t
    rendered_cell_colors[CONSOLE_MAX_ROWS][CONSOLE_MAX_COLS];

static uint32_t console_cols = 1;
static uint32_t console_rows = 1;

static uint32_t cur_row = 0;
static uint32_t cur_col = 0;

static uint64_t text_x = 0;
static uint64_t text_y = 0;

static bool console_dirty = true;
static bool console_rendered = false;
static uint32_t rendered_cursor_row = 0;
static uint32_t rendered_cursor_col = 0;
static bool console_input_active = false;
static uint64_t last_console_input_tick = 0;

#define CONSOLE_INPUT_SETTLE_TICKS 25

static char command_buffer[128];
static uint32_t command_length = 0;

/* ================= MOUSE STATE ================= */

static bool mouse_ready = false;

static void hide_cursor_if_overlapping(
    struct limine_framebuffer *fb,
    gfx_rect_t rect
)
{
    if (mouse_ready &&
        mouse_cursor_overlaps(
            rect.x,
            rect.y,
            rect.width,
            rect.height
        )) {
        mouse_cursor_hide(fb);
    }
}

/* ================= CONSOLE CLEAR ================= */

static void console_erase_all(void)
{
    for (uint32_t r = 0;
         r < CONSOLE_MAX_ROWS; r++) {

        for (uint32_t c = 0;
             c < CONSOLE_MAX_COLS; c++) {

            cells[r][c] = 0;
            cell_colors[r][c] = COLOR_NORMAL;
        }
    }

    cur_row = 0;
    cur_col = 0;

    console_dirty = true;
}

/* ================= CONSOLE SCROLLING ================= */

static void console_newline(void)
{
    cur_col = 0;

    if (++cur_row >= console_rows) {
        for (uint32_t r = 1;
             r < console_rows; r++) {

            for (uint32_t c = 0;
                 c < console_cols; c++) {

                cells[r - 1][c] = cells[r][c];

                cell_colors[r - 1][c] =
                    cell_colors[r][c];
            }
        }

        cur_row = console_rows - 1;

        for (uint32_t c = 0;
             c < console_cols; c++) {

            cells[cur_row][c] = 0;

            cell_colors[cur_row][c] =
                COLOR_NORMAL;
        }
    }

    console_dirty = true;
}

/* ================= CONSOLE OUTPUT ================= */

static void console_char(
    char ch,
    uint32_t color
)
{
    if (ch == '\n') {
        console_newline();
        return;
    }

    if (cur_col >= console_cols)
        console_newline();

    cells[cur_row][cur_col] = ch;
    cell_colors[cur_row][cur_col] = color;

    cur_col++;

    if (cur_col == console_cols)
        console_newline();

    console_dirty = true;
}

static void console_write(
    const char *text,
    uint32_t color
)
{
    while (*text)
        console_char(*text++, color);
}

static void console_line(
    const char *text,
    uint32_t color
)
{
    console_write(text, color);
    console_newline();
}

static void console_prompt(void)
{
    console_write("MYOS> ", COLOR_PROMPT);
}

/* ================= CONSOLE BACKSPACE ================= */

static void console_backspace(void)
{
    if (cur_col == 0) {
        if (cur_row == 0)
            return;

        cur_row--;
        cur_col = console_cols;
    }

    --cur_col;

    cells[cur_row][cur_col] = 0;
    cell_colors[cur_row][cur_col] = COLOR_NORMAL;

    console_dirty = true;
}

/* ================= CONSOLE INITIALIZATION ================= */

static void console_init(
    struct limine_framebuffer *fb
)
{
    gfx_rect_t w = graphics_console_rect(fb);

    text_x = w.x + 12;
    text_y = w.y + 35;

    uint64_t available_w =
        w.width > 26 ?
        w.width - 26 : CHAR_STEP;

    uint64_t available_h =
        w.height > 49 ?
        w.height - 49 : LINE_STEP;

    console_cols =
        (uint32_t)(available_w / CHAR_STEP);

    console_rows =
        (uint32_t)(available_h / LINE_STEP);

    if (console_cols < 1)
        console_cols = 1;

    if (console_rows < 1)
        console_rows = 1;

    if (console_cols > CONSOLE_MAX_COLS)
        console_cols = CONSOLE_MAX_COLS;

    if (console_rows > CONSOLE_MAX_ROWS)
        console_rows = CONSOLE_MAX_ROWS;

    console_erase_all();

    console_line(
        "WELCOME TO MYOS",
        COLOR_HEADING
    );

    console_line(
        "TYPE HELP FOR COMMANDS",
        COLOR_MUTED
    );

    console_newline();
    console_prompt();
}

/* ================= CONSOLE RENDERING ================= */

static void console_render(
    struct limine_framebuffer *fb
)
{
    if (!console_dirty ||
        graphics_get_console_state() !=
        GFX_CONSOLE_OPEN) {
        return;
    }

    bool full_redraw = !console_rendered;

    if (full_redraw) {
        graphics_clear_console_body(fb);
    } else if (rendered_cursor_col < console_cols) {
        graphics_rect(
            fb,
            text_x + rendered_cursor_col * CHAR_STEP,
            text_y + rendered_cursor_row * LINE_STEP + 15,
            9,
            2,
            COLOR_BG
        );
    }

    uint64_t right =
        text_x + (uint64_t)console_cols * CHAR_STEP;

    uint64_t bottom =
        text_y + (uint64_t)console_rows * LINE_STEP;

    for (uint32_t r = 0;
         r < console_rows; r++) {

        for (uint32_t c = 0;
             c < console_cols; c++) {

            char ch = cells[r][c];

            if (!full_redraw &&
                ch == rendered_cells[r][c] &&
                cell_colors[r][c] ==
                    rendered_cell_colors[r][c]) {
                continue;
            }

            graphics_rect(
                fb,
                text_x + c * CHAR_STEP,
                text_y + r * LINE_STEP,
                CHAR_STEP,
                14,
                COLOR_BG
            );

            if (ch && ch != ' ') {
                draw_scaled_char(
                    fb,
                    text_x + c * CHAR_STEP,
                    text_y + r * LINE_STEP,
                    ch,
                    cell_colors[r][c],
                    right,
                    bottom
                );
            }

            rendered_cells[r][c] = ch;
            rendered_cell_colors[r][c] = cell_colors[r][c];
        }
    }

    // Lavender typing cursor.
    if (cur_col < console_cols) {
        graphics_rect(
            fb,
            text_x + cur_col * CHAR_STEP,
            text_y + cur_row * LINE_STEP + 15,
            9,
            2,
            COLOR_PROMPT
        );
    }

    rendered_cursor_row = cur_row;
    rendered_cursor_col = cur_col;
    console_rendered = true;
    console_dirty = false;
}

/* ================= KEYBOARD INPUT QUEUE ================= */

/*
 * The keyboard IRQ only stores input here.
 *
 * The main kernel loop processes commands
 * and updates the screen.
 */

static volatile uint32_t input_head = 0;
static volatile uint32_t input_tail = 0;

static char input_queue[INPUT_QUEUE_SIZE];

void keyboard_put_char(char ch)
{
    uint32_t next =
        (input_head + 1) % INPUT_QUEUE_SIZE;

    if (next == input_tail)
        return;

    input_queue[input_head] = ch;

    asm volatile ("" ::: "memory");

    input_head = next;
}

static bool input_pop(char *ch)
{
    if (input_tail == input_head)
        return false;

    *ch = input_queue[input_tail];

    input_tail =
        (input_tail + 1) % INPUT_QUEUE_SIZE;

    return true;
}

/* ================= TASK DEMONSTRATION ================= */

static volatile bool demo_running = false;
static volatile bool demo_started = false;
static volatile bool demo_redraw = true;

static volatile uint64_t power_a = 4;
static volatile uint64_t power_b = 9;

static volatile uint32_t exponent_a = 2;
static volatile uint32_t exponent_b = 2;

static volatile uint64_t last_tick_a = 0;
static volatile uint64_t last_tick_b = 0;

static void demo_start(void)
{
    scheduler_set_demo_running(false);
    demo_running = false;

    power_a = 4;
    power_b = 9;

    exponent_a = 2;
    exponent_b = 2;

    last_tick_a = last_tick_b = timer_ticks();

    demo_started = true;
    demo_redraw = true;

    demo_running = true;

    scheduler_set_demo_running(true);
}

static void demo_stop(void)
{
    demo_running = false;
    scheduler_set_demo_running(false);

    demo_redraw = true;
}

/* ================= TASK A ================= */

static void task_a(void)
{
    for (;;) {
        if (demo_running) {
            uint64_t now = timer_ticks();

            if (now - last_tick_a >=
                    POWER_INTERVAL_TICKS &&
                exponent_a < 63) {

                power_a *= 2;
                exponent_a++;
                last_tick_a = now;
            }
        }

        asm volatile ("pause");
    }
}

/* ================= TASK B ================= */

static void task_b(void)
{
    for (;;) {
        if (demo_running) {
            uint64_t now = timer_ticks();

            if (now - last_tick_b >=
                    POWER_INTERVAL_TICKS &&
                exponent_b < 40) {

                power_b *= 3;
                exponent_b++;
                last_tick_b = now;
            }
        }

        asm volatile ("pause");
    }
}

/* ================= LIVE TASK DISPLAY ================= */

static void demo_render(
    struct limine_framebuffer *fb
)
{
    static uint64_t previous_a = 0;
    static uint64_t previous_b = 0;

    static uint32_t previous_exp_a = 0;
    static uint32_t previous_exp_b = 0;

    static bool previous_running = false;
    static bool previous_started = false;

    bool started = demo_started;
    bool running = demo_running;

    uint64_t a = power_a;
    uint64_t b = power_b;

    uint32_t ea = exponent_a;
    uint32_t eb = exponent_b;

    if (!demo_redraw &&
        previous_started == started &&
        previous_running == running &&
        previous_a == a &&
        previous_b == b &&
        previous_exp_a == ea &&
        previous_exp_b == eb) {
        return;
    }

    if (graphics_get_console_state() !=
        GFX_CONSOLE_OPEN) {
        return;
    }

    graphics_rect(
        fb, 94, 24, 510, 66, COLOR_BG
    );

    if (started) {
        uint64_t xmax =
            fb->width < 604 ? fb->width : 604;

        uint64_t ymax =
            fb->height < 90 ? fb->height : 90;

        draw_label(
            fb, 100, 29,
            running ?
            "TASKS RUNNING" : "TASKS STOPPED",
            running ?
            COLOR_SUCCESS : COLOR_ERROR,
            xmax, ymax
        );

        char digits[21];
        uint64_t x;

        // Task A
        x = draw_label(
            fb, 100, 49,
            "A  2 ^ ",
            COLOR_HEADING,
            xmax, ymax
        );

        uint_to_string(ea, digits);

        x = draw_label(
            fb, x, 49, digits,
            COLOR_NORMAL, xmax, ymax
        );

        x = draw_label(
            fb, x, 49, " = ",
            COLOR_HEADING, xmax, ymax
        );

        uint_to_string(a, digits);

        draw_label(
            fb, x, 49, digits,
            COLOR_NORMAL, xmax, ymax
        );

        // Task B
        x = draw_label(
            fb, 100, 69,
            "B  3 ^ ",
            COLOR_HEADING,
            xmax, ymax
        );

        uint_to_string(eb, digits);

        x = draw_label(
            fb, x, 69, digits,
            COLOR_NORMAL, xmax, ymax
        );

        x = draw_label(
            fb, x, 69, " = ",
            COLOR_HEADING, xmax, ymax
        );

        uint_to_string(b, digits);

        draw_label(
            fb, x, 69, digits,
            COLOR_NORMAL, xmax, ymax
        );
    }

    demo_redraw = false;

    previous_started = started;
    previous_running = running;

    previous_a = a;
    previous_b = b;

    previous_exp_a = ea;
    previous_exp_b = eb;
}

/* ================= COMMAND EXECUTION ================= */

static void shell_run_command(void)
{
    command_buffer[command_length] = '\0';

    // Every command except tasks stops the demo.
    if (!string_equals(command_buffer, "tasks"))
        demo_stop();

    if (string_equals(command_buffer, "help")) {
        console_line(
            "AVAILABLE COMMANDS",
            COLOR_HEADING
        );

        console_line(
            "HELP CLEAR ABOUT VERSION",
            COLOR_NORMAL
        );

        console_line(
            "ECHO REBOOT CPU MEMORY",
            COLOR_NORMAL
        );

        console_line(
            "TOUCH LS CAT WRITE RM",
            COLOR_NORMAL
        );

        console_line(
            "TICKS UPTIME TASKS MOUSE",
            COLOR_NORMAL
        );
    }
    else if (starts_with(command_buffer, "echo ")) {
        console_line(
            command_buffer + 5,
            COLOR_NORMAL
        );
    }
    else if (string_equals(command_buffer, "about")) {
        console_line(
            "MYOS CUSTOM X86 64 OS",
            COLOR_NORMAL
        );
    }
    else if (string_equals(command_buffer, "version")) {
        console_line(
            "MYOS VERSION 0 1",
            COLOR_NORMAL
        );
    }
    else if (string_equals(command_buffer, "clear")) {
        console_erase_all();
        demo_started = false;
        demo_redraw = true;
    }
    else if (string_equals(command_buffer, "reboot")) {
        asm volatile (
            "cli; movb $0xFE, %%al; "
            "outb %%al, $0x64"
            :
            :
            : "al", "memory"
        );

        for (;;)
            asm volatile ("hlt");
    }
    else if (string_equals(command_buffer, "cpu")) {
        char vendor[13];

        get_cpu_vendor(vendor);

        console_line(
            "CPU VENDOR",
            COLOR_HEADING
        );

        console_line(
            vendor,
            COLOR_NORMAL
        );
    }
    else if (string_equals(command_buffer, "memory")) {
        uint64_t total = 0;

        if (memmap_request.response) {
            for (uint64_t i = 0;
                 i < memmap_request.response->entry_count;
                 i++) {

                struct limine_memmap_entry *e =
                    memmap_request.response->entries[i];

                if (e->type == LIMINE_MEMMAP_USABLE)
                    total += e->length;
            }
        }

        print_value_line(
            "USABLE RAM MB",
            total / (1024 * 1024)
        );
    }


    /* ================= RAM FILESYSTEM ================= */
    else if (starts_with(command_buffer, "touch ")) {
        bool ok = ramfs_create(command_buffer + 6);
        if (ok)
            desktop_files_mark_dirty();
        print_result(ok, "FILE CREATED", "CREATE FAILED");
    }
    else if (string_equals(command_buffer, "ls")) {
        bool found = false;

        for (size_t i = 0;
             i < RAMFS_MAX_FILES; i++) {

            const char *name = ramfs_name(i);

            if (name) {
                console_line(name, COLOR_NORMAL);
                found = true;
            }
        }

        if (!found)
            console_line("NO FILES", COLOR_MUTED);
    }
    else if (starts_with(command_buffer, "cat ")) {
        const char *data =
            ramfs_read(command_buffer + 4);

        console_line(
            data ? data : "FILE NOT FOUND",
            data ? COLOR_NORMAL : COLOR_ERROR
        );
    }
    else if (starts_with(command_buffer, "rm ")) {
        bool ok = ramfs_delete(command_buffer + 3);
        if (ok) {

            desktop_files_mark_dirty();

            if (file_viewer_displays(command_buffer + 3)) {
            file_viewer_close();
            viewer_minimized = false;
            gui_redraw_needed = true;
            }
        }
        print_result(ok, "FILE DELETED", "FILE NOT FOUND");
    }
    else if (starts_with(command_buffer, "write ")) {
        char *name = command_buffer + 6;
        char *data = name;

        while (*data && *data != ' ')
            data++;

        if (!*data) {
            console_line(
                "USAGE WRITE NAME TEXT",
                COLOR_ERROR
            );
        }
        else {
            *data++ = '\0';

            bool ok = ramfs_write(name, data);
            if (ok && file_viewer_displays(name)) {
                file_viewer_mark_dirty();
            }
            print_result(ok, "FILE UPDATED", "WRITE FAILED");
        }
    }

    /* ================= TIMER ================= */
    else if (string_equals(command_buffer, "ticks")) {
        print_value_line("TIMER TICKS", timer_ticks());
    }
    else if (string_equals(command_buffer, "uptime")) {
        print_value_line("UPTIME SECONDS", timer_uptime());
    }

    /* ================= SCHEDULER ================= */
    else if (string_equals(command_buffer, "tasks")) {
        demo_start();

        console_line(
            "TASK DEMO STARTED",
            COLOR_SUCCESS
        );
    }
    else if (string_equals(command_buffer, "mouse")) {

    char buffer[21];
    uint64_t x, y;

    mouse_cursor_position(&x, &y);

    console_line(
        mouse_ready ? "MOUSE READY" : "MOUSE NOT AVAILABLE",
        mouse_ready ? COLOR_SUCCESS : COLOR_ERROR
    );

    console_line("CURSOR X", COLOR_HEADING);

    uint_to_string(x, buffer);
    console_line(buffer, COLOR_NORMAL);

    console_line("CURSOR Y", COLOR_HEADING);

    uint_to_string(y, buffer);
    console_line(buffer, COLOR_NORMAL);

    console_line("LEFT CLICKS", COLOR_HEADING);

    uint_to_string(mouse_left_clicks(), buffer);
    console_line(buffer, COLOR_NORMAL);
    }
    else if (command_length) {
        console_line(
            "UNKNOWN COMMAND",
            COLOR_ERROR
        );
    }

    command_length = 0;
    console_prompt();
}

/* ================= KEYBOARD PROCESSING ================= */

static void shell_handle_char(char ch)
{
    last_console_input_tick = timer_ticks();
    console_input_active = true;

    if (ch == '\r')
        ch = '\n';

    if (ch == '\n') {
        console_newline();
        shell_run_command();
    }
    else if (ch == '\b') {
        if (command_length) {
            command_length--;
            console_backspace();
        }
    }

    else if (ch >= 32 &&
             ch <= 126 &&
             command_length <
             sizeof(command_buffer) - 1) {

        command_buffer[command_length++] = ch;

        console_char(ch, COLOR_COMMAND);
    }
}

/* ================= KERNEL BOOTSTRAP ================= */

static void kernel_bootstrap(void)
{
    heap_init();
    ramfs_init();
    file_viewer_init();

    void *test1 = kmalloc(128);
    void *test2 = kmalloc(256);

    if (!test1 || !test2)
        hcf();

    kfree(test1);
    kfree(test2);

    scheduler_init();

    int a = task_create(task_a);
    int b = task_create(task_b);

    if (a < 0 || b < 0)
        hcf();

    scheduler_set_demo_running(false);

    idt_init();
    pic_remap(hhdm_request.response->offset);
    timer_init();
    mouse_ready = mouse_init();

    if (mouse_ready) {
        pic_enable_mouse_irq();
    }
}


static void handle_mouse_click(
    struct limine_framebuffer *fb
)
{
    uint64_t mouse_x;
    uint64_t mouse_y;

    while (mouse_cursor_pop_click(
        &mouse_x,
        &mouse_y
    )) {

        /* ============== TASKBAR INPUT ============== */

        size_t selected_file = 0;

        taskbar_action_t action = taskbar_click(
            fb,
            mouse_x,
            mouse_y,
            file_viewer_is_open(),
            &selected_file
        );

        if (action != TASKBAR_NONE) {

            switch (action) {

                case TASKBAR_CONSOLE:

                    // Restore console and switch away
                    // from the file viewer.
                    graphics_set_console_state(
                        GFX_CONSOLE_OPEN
                    );

                    viewer_minimized = true;
                    break;

                case TASKBAR_VIEWER:

                    if (file_viewer_is_open()) {
                        viewer_minimized = false;
                    }
                    break;

                case TASKBAR_DESKTOP:

                    // Hide both windows but preserve
                    // their contents and state.
                    graphics_set_console_state(
                        GFX_CONSOLE_MINIMIZED
                    );

                    viewer_minimized = true;
                    break;

                case TASKBAR_ABOUT:

                    graphics_set_console_state(
                        GFX_CONSOLE_OPEN
                    );

                    viewer_minimized = true;

                    console_newline();

                    console_line(
                        "MYOS CUSTOM X86 64",
                        COLOR_HEADING
                    );

                    console_line(
                        "RETRO KERNEL DESKTOP",
                        COLOR_NORMAL
                    );

                    console_line(
                        "RAMFS SCHEDULER MOUSE GUI",
                        COLOR_SUCCESS
                    );

                    console_prompt();
                    break;

                case TASKBAR_FILE: {

                    const char *name =
                        ramfs_name(selected_file);

                    if (name &&
                        file_viewer_open(name)) {

                        viewer_minimized = false;
                    }

                    break;
                }

                case TASKBAR_REBOOT:

                    asm volatile (
                        "cli\n"
                        "movb $0xFE, %%al\n"
                        "outb %%al, $0x64\n"
                        :
                        :
                        : "al", "memory"
                    );

                    for (;;)
                        asm volatile ("hlt");

                default:
                    // Menu opened, closed or changed.
                    break;
            }

            gui_redraw_needed = true;
            continue;
        }

        /* ============== FILE VIEWER INPUT ============== */

        if (file_viewer_is_open() &&
            !viewer_minimized) {

            if (file_viewer_handle_click(
                fb,
                mouse_x,
                mouse_y
            )) {

                // Closing or scrolling the viewer
                // requires refreshing its appearance.
                if (!file_viewer_is_open()) {
                    viewer_minimized = false;
                }

                gui_redraw_needed = true;
                continue;
            }
        }

        /* ============== CONSOLE CONTROLS ============== */

        gfx_console_state_t state =
            graphics_get_console_state();

        if (state == GFX_CONSOLE_OPEN) {

            if (graphics_point_in_rect(
                graphics_close_button_rect(fb),
                mouse_x,
                mouse_y
            )) {

                graphics_set_console_state(
                    GFX_CONSOLE_CLOSED
                );

                gui_redraw_needed = true;
                continue;
            }

            if (graphics_point_in_rect(
                graphics_minimize_button_rect(fb),
                mouse_x,
                mouse_y
            )) {

                graphics_set_console_state(
                    GFX_CONSOLE_MINIMIZED
                );

                gui_redraw_needed = true;
                continue;
            }

            if (graphics_point_in_rect(
                graphics_console_rect(fb),
                mouse_x,
                mouse_y
            )) {

                // Console receives focus.
                // Viewer can be restored from taskbar.
                if (file_viewer_is_open() &&
                    !viewer_minimized) {

                    viewer_minimized = true;
                    gui_redraw_needed = true;
                }

                continue;
            }
        }

        /* ============== DESKTOP FILE ICONS ============== */

        const char *filename =
            desktop_files_hit_test(
                fb,
                mouse_x,
                mouse_y
            );

        if (filename) {

            if (file_viewer_open(filename)) {

                viewer_minimized = false;
                gui_redraw_needed = true;
            }
        }
    }
}



static void kernel_run(struct limine_framebuffer *fb)
{
    /* ============== INITIAL GRAPHICS ============== */

    // Initialize taskbar.
    taskbar_init();

    // Initialize companion animation.
    companion_init(timer_ticks());

    // Draw wallpaper and desktop.
    graphics_draw_desktop(fb);

    // Draw RAMFS desktop file icons.
    desktop_files_draw(fb);
    desktop_files_clear_dirty();

    // Initialize console.
    console_init(fb);

    // Initialize mouse cursor.
    if (mouse_ready) {
        mouse_cursor_init(fb);
    }

    // Draw console.
    console_render(fb);

    // Draw taskbar.
    taskbar_draw(
        fb,
        timer_uptime(),
        true,
        file_viewer_is_open(),
        false
    );

    // Draw companion LAST, above all windows.
    companion_draw(fb);

    // Enable hardware interrupts.
    asm volatile ("sti" ::: "memory");

    /* ============== TASK RENDER TRACKING ============== */

    uint64_t previous_power_a = power_a;
    uint64_t previous_power_b = power_b;

    /* ============== MAIN KERNEL LOOP ============== */

    for (;;) {

        /* 2. PROCESS KEYBOARD INPUT */

        char ch;

        while (input_pop(&ch)) {
            shell_handle_char(ch);
        }

        /* 3. PROCESS MOUSE INPUT */

        if (mouse_ready) {
            mouse_cursor_process(fb);

            // Console controls, launcher,
            // desktop icons and viewer clicks.
            handle_mouse_click(fb);
        }

        /* 4. UPDATE COMPANION ANIMATION */

        bool console_open =
            graphics_get_console_state() ==
                GFX_CONSOLE_OPEN;

        uint64_t ticks = timer_ticks();

        bool companion_changed =
            companion_update(
                ticks,
                console_open
            );

        /* 5. CHECK FOR DESKTOP CHANGES */

        bool full_redraw =
            gui_redraw_needed ||
            desktop_files_needs_redraw();

        bool typing_active =
            console_input_active &&
            !full_redraw &&
            ticks - last_console_input_tick <
                CONSOLE_INPUT_SETTLE_TICKS;

        bool console_was_dirty =
            (console_dirty || full_redraw) &&
            console_open &&
            !typing_active;

        bool task_status_changed =
            full_redraw ||
            (console_open &&
             (demo_redraw ||
              power_a != previous_power_a ||
              power_b != previous_power_b));

        bool viewer_front =
            file_viewer_is_open() &&
            !viewer_minimized;

        bool viewer_will_redraw =
            viewer_front &&
            (file_viewer_needs_redraw() ||
             full_redraw ||
             console_was_dirty);

        uint64_t uptime = timer_uptime();
        bool taskbar_will_redraw =
            full_redraw ||
            taskbar_needs_draw(uptime) ||
            console_was_dirty ||
            viewer_will_redraw;

        bool companion_underlay_will_redraw =
            full_redraw ||
            console_was_dirty ||
            task_status_changed ||
            viewer_will_redraw;

        bool redraw_companion =
            companion_changed ||
            companion_underlay_will_redraw;

        if (redraw_companion) {
            if (full_redraw && mouse_ready)
                mouse_cursor_hide(fb);

            if (!full_redraw) {
                hide_cursor_if_overlapping(
                    fb,
                    companion_bounds(fb, console_open)
                );
            }

            if (companion_underlay_will_redraw)
                companion_restore_background(fb);
        }

        if (full_redraw) {
            // Rebuild wallpaper and window frames.
            graphics_draw_desktop(fb);

            // Restore all RAMFS file icons.
            desktop_files_draw(fb);
            desktop_files_clear_dirty();

            // Request fresh console rendering.
            console_dirty = true;
            console_rendered = false;

            // Request task status repaint.
            demo_redraw = true;

            // Refresh file viewer if open.
            file_viewer_mark_dirty();

            // Refresh taskbar and launcher.
            taskbar_invalidate();

            gui_redraw_needed = false;
        }

        /* 6. RENDER CONSOLE */

        if (console_was_dirty) {
            hide_cursor_if_overlapping(
                fb,
                graphics_console_rect(fb)
            );
        }

        console_render(fb);
        if (console_was_dirty)
            console_input_active = false;

        /* 7. RENDER SCHEDULER TASK STATUS */

        previous_power_a = power_a;
        previous_power_b = power_b;

        if (task_status_changed) {
            hide_cursor_if_overlapping(
                fb,
                (gfx_rect_t){94, 24, 510, 66}
            );
        }

        demo_render(fb);

        /* 8. RENDER FILE VIEWER */

        bool viewer_redrew = false;

        if (viewer_will_redraw) {

            hide_cursor_if_overlapping(
                fb,
                file_viewer_bounds(fb)
            );

            file_viewer_draw(fb);
            viewer_redrew = true;
        }

        /* 9. RENDER FUNCTIONAL TASKBAR */

        bool taskbar_redrew =
            full_redraw ||
            taskbar_will_redraw ||
            console_was_dirty ||
            viewer_redrew;

        if (taskbar_redrew) {
            hide_cursor_if_overlapping(
                fb,
                (gfx_rect_t){
                    0,
                    fb->height > 42 ? fb->height - 42 : 0,
                    fb->width,
                    fb->height < 42 ? fb->height : 42
                }
            );

            taskbar_draw(
                fb,
                uptime,
                console_open,
                file_viewer_is_open(),
                viewer_front
            );
        }

        /* 10. DRAW COMPANION IN FOREGROUND */

        /*
         * Draw her AFTER:
         * - Desktop icons
         * - Console
         * - Scheduler status
         * - File viewer
         * - Taskbar and launcher
         *
         * This ensures she always appears
         * in front of the GUI.
         */

        if (redraw_companion) {

            hide_cursor_if_overlapping(
                fb,
                companion_bounds(fb, console_open)
            );

            companion_draw(fb);
        }

        /* 11. DRAW MOUSE CURSOR LAST */

        // Mouse always appears above companion.
        if (mouse_ready) {
            mouse_cursor_show(fb);
        }

        /* 12. WAIT FOR INTERRUPT */

        asm volatile ("hlt" ::: "memory");
    }
}




/* ================= KERNEL MAIN ================= */

void kmain(void)
{
    if (!LIMINE_BASE_REVISION_SUPPORTED(base_revision) ||
        !framebuffer_request.response ||
        !hhdm_request.response) {
        hcf();
    }

    kernel_bootstrap();
    kernel_run(framebuffer_request.response->framebuffers[0]);
}
