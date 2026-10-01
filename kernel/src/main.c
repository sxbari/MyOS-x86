#include <stdint.h>
#include <stdbool.h>
#include <limine.h>
#include <stddef.h>
#include <stddef.h>

void idt_init(void);
void pic_remap(uint64_t hhdm_offset);

__attribute__((used, section(".limine_requests")))
static volatile uint64_t base_revision[] = LIMINE_BASE_REVISION(6);

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
static volatile uint64_t requests_start[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t requests_end[] = LIMINE_REQUESTS_END_MARKER;

static void hcf(void)
{
    for (;;)
    {
        asm ("hlt");
    }
}
static void draw_char(
    struct limine_framebuffer *framebuffer,
    uint64_t x,
    uint64_t y,
    char c
)
{
    uint32_t *pixels = framebuffer->address;

    uint8_t letters[36][5] = {
        {0x1E, 0x05, 0x1F, 0x15, 0x15}, // A
        {0x1E, 0x15, 0x1E, 0x15, 0x1E}, // B
        {0x1F, 0x10, 0x10, 0x10, 0x1F}, // C
        {0x1E, 0x15, 0x15, 0x15, 0x1E}, // D
        {0x1F, 0x10, 0x1E, 0x10, 0x1F}, // E
        {0x1F, 0x10, 0x1E, 0x10, 0x10}, // F
        {0x1F, 0x10, 0x17, 0x15, 0x1F}, // G
        {0x15, 0x15, 0x1F, 0x15, 0x15}, // H
        {0x1F, 0x04, 0x04, 0x04, 0x1F}, // I
        {0x01, 0x01, 0x01, 0x11, 0x0E}, // J
        {0x15, 0x16, 0x1C, 0x16, 0x15}, // K
        {0x10, 0x10, 0x10, 0x10, 0x1F}, // L
        {0x11, 0x1B, 0x15, 0x11, 0x11}, // M
        {0x11, 0x19, 0x15, 0x13, 0x11}, // N
        {0x0E, 0x11, 0x11, 0x11, 0x0E}, // O
        {0x1E, 0x11, 0x1E, 0x10, 0x10}, // P
        {0x0E, 0x11, 0x11, 0x15, 0x0F}, // Q
        {0x1E, 0x11, 0x1E, 0x14, 0x12}, // R
        {0x0F, 0x10, 0x0E, 0x01, 0x1E}, // S
        {0x1F, 0x04, 0x04, 0x04, 0x04}, // T
        {0x11, 0x11, 0x11, 0x11, 0x0E}, // U
        {0x11, 0x11, 0x11, 0x0A, 0x04}, // V
        {0x11, 0x11, 0x15, 0x1B, 0x11}, // W
        {0x11, 0x0A, 0x04, 0x0A, 0x11}, // X
        {0x11, 0x0A, 0x04, 0x04, 0x04}, // Y
        {0x1F, 0x02, 0x04, 0x08, 0x1F}, // Z
        {0x0E, 0x11, 0x15, 0x11, 0x0E}, // 0
        {0x04, 0x0C, 0x04, 0x04, 0x0E}, // 1
        {0x0E, 0x11, 0x02, 0x04, 0x1F}, // 2
        {0x1E, 0x01, 0x06, 0x01, 0x1E}, // 3
        {0x02, 0x06, 0x0A, 0x1F, 0x02}, // 4
        {0x1F, 0x10, 0x1E, 0x01, 0x1E}, // 5
        {0x0E, 0x10, 0x1E, 0x11, 0x0E}, // 6
        {0x1F, 0x01, 0x02, 0x04, 0x08}, // 7
        {0x0E, 0x11, 0x0E, 0x11, 0x0E}, // 8
        {0x0E, 0x11, 0x0F, 0x01, 0x0E}  // 9
    };

    if (c >= 'a' && c <= 'z')
        c -= 'a' - 'A';

    uint64_t glyph_index;
    if (c >= 'A' && c <= 'Z')
        glyph_index = c - 'A';
    else if (c >= '0' && c <= '9')
        glyph_index = 26 + c - '0';
    else
        return;

    uint8_t *letter = letters[glyph_index];

    for (uint64_t row = 0; row < 5; row++)
    {
        for (uint64_t col = 0; col < 5; col++)
        {
            if (letter[row] & (1 << (4 - col)))
            {
                pixels[(y + row) * framebuffer->pitch / 4 + (x + col)] =
                    0x00FFFFFF;
            }
        }
    }
}

void keyboard_put_char(char character)
{
    static uint64_t cursor_x = 100;
    static uint64_t cursor_y = 120;

    struct limine_framebuffer *framebuffer =
        framebuffer_request.response->framebuffers[0];

    if (character == '\n')
    {
        cursor_x = 100;
        cursor_y += 7;
    }
    else if (character == '\b')
    {
        if (cursor_x > 100)
            cursor_x -= 6;
        else if (cursor_y > 120)
        {
            cursor_y -= 7;
            cursor_x = framebuffer->width >= 106 ? framebuffer->width - 6 : 100;
        }
        else
            return;

        uint32_t *pixels = framebuffer->address;
        for (uint64_t y = 0; y < 5; y++)
        {
            for (uint64_t x = 0; x < 5; x++)
                pixels[(cursor_y + y) * framebuffer->pitch / 4 + cursor_x + x] =
                    0x0000FF00;
        }
    }
    else
    {
        if (character != ' ')
            draw_char(framebuffer, cursor_x, cursor_y, character);

        cursor_x += 6;
        if (cursor_x + 5 >= framebuffer->width)
        {
            cursor_x = 100;
            cursor_y += 7;
        }
    }

    if (cursor_y + 5 >= framebuffer->height)
        cursor_y = 120;
}

static void draw_text(
    struct limine_framebuffer *framebuffer,
    uint64_t x,
    uint64_t y,
    const char *text
)
{
    while (*text)
    {
        draw_char(framebuffer, x, y, *text);

        x += 6;
        text++;
    }
}

void kmain(void)
{
    if (!LIMINE_BASE_REVISION_SUPPORTED(base_revision))
    {
        hcf();
    }

    if (framebuffer_request.response == NULL || hhdm_request.response == NULL)
    {
        hcf();
    }

    idt_init();
    pic_remap(hhdm_request.response->offset);
    

    struct limine_framebuffer *framebuffer =
        framebuffer_request.response->framebuffers[0];

    uint32_t *pixels = framebuffer->address;

    for (uint64_t y = 0; y < framebuffer->height; y++)
    {
        for (uint64_t x = 0; x < framebuffer->width; x++)
        {
            pixels[y * framebuffer->pitch / 4 + x] = 0x0000FF00;
        }
    }
    draw_text(framebuffer, 100, 100, "HELLO FROM MYOS");

    asm volatile ("sti");
    hcf();
}