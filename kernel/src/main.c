#include <stdint.h>
#include <stdbool.h>
#include <limine.h>
#include <stddef.h>
#include <stddef.h>
#include "memory/heap.h"
#include "filesystem/ramfs.h"
#include "interrupts/timer.h"
#include "scheduler/scheduler.h"
#define BACKGROUND_COLOR 0x0030104F

void idt_init(void);
void pic_remap(uint64_t hhdm_offset);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

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
static char command_buffer[128];
static int command_length = 0;

static bool string_equals(const char *a, const char *b)
{
    while (*a && *b)
    {
        if (*a != *b)
            return false;

        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}
static bool starts_with(const char *text, const char *prefix)
{
    while (*prefix) {
        if (*text != *prefix)
            return false;
        text++;
        prefix++;
    }
    return true;
}
static void draw_text(
    struct limine_framebuffer *framebuffer,
    uint64_t x,
    uint64_t y,
    const char *text
);
static void get_cpu_vendor(char vendor[13])
{
    uint32_t eax, ebx, ecx, edx;

    asm volatile (
        "cpuid"
        : "=a"(eax), "=b"(ebx),
          "=c"(ecx), "=d"(edx)
        : "a"(0)
    );

    for (int i = 0; i < 4; i++)
    {
        vendor[i]     = (ebx >> (i * 8)) & 0xFF;
        vendor[i + 4] = (edx >> (i * 8)) & 0xFF;
        vendor[i + 8] = (ecx >> (i * 8)) & 0xFF;
    }

    vendor[12] = '\0';

}
static void uint_to_string(uint64_t number, char *buffer)
{
    char temp[21];
    int i = 0;
    int j = 0;

    if (number == 0)
    {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    while (number > 0)
    {
        temp[i++] = '0' + (number % 10);
        number /= 10;
    }

    while (i > 0)
        buffer[j++] = temp[--i];

    buffer[j] = '\0';
}
void keyboard_put_char(char character)
{
    static uint64_t cursor_x = 100;
    static uint64_t cursor_y = 120;

    struct limine_framebuffer *framebuffer =
        framebuffer_request.response->framebuffers[0];

    if (character == '\n')
{
    command_buffer[command_length] = '\0';

    cursor_x = 100;
    cursor_y += 14;

    if (string_equals(command_buffer, "help"))
{
    draw_text(framebuffer, 100, cursor_y,
              "HELP CLEAR ABOUT VERSION");
    cursor_y += 14;

    draw_text(framebuffer, 100, cursor_y,
              "ECHO REBOOT CPU MEMORY");
    cursor_y += 14;

    draw_text(framebuffer, 100, cursor_y,
              "TOUCH LS CAT WRITE RM");
    cursor_y += 14;

    draw_text(framebuffer, 100, cursor_y,
              "TICKS UPTIME");
    cursor_y += 14;
}
    else if (command_buffer[0] == 'e' &&
         command_buffer[1] == 'c' &&
         command_buffer[2] == 'h' &&
         command_buffer[3] == 'o' &&
         command_buffer[4] == ' ')
{
    draw_text(framebuffer, 100, cursor_y, command_buffer + 5);
    cursor_y += 14;
}
    else if (string_equals(command_buffer, "about"))
    {
        draw_text(framebuffer, 100, cursor_y, "MYOS CUSTOM X86 64 OS");
        cursor_y += 14;
    }
    else if (string_equals(command_buffer, "version"))
    {
        draw_text(framebuffer, 100, cursor_y, "MYOS VERSION 0 1");
        cursor_y += 14;
    }
    else if (string_equals(command_buffer, "clear"))
    {
        uint32_t *pixels = framebuffer->address;

        for (uint64_t y = 0; y < framebuffer->height; y++)
            for (uint64_t x = 0; x < framebuffer->width; x++)
                pixels[y * framebuffer->pitch / 4 + x] = 0x0030104F;

        cursor_y = 120;
    }
    else if (string_equals(command_buffer, "reboot"))
{
    asm volatile (
        "cli\n"
        "movb $0xFE, %%al\n"
        "outb %%al, $0x64\n"
        :
        :
        : "al"
    );

    for (;;)
        asm volatile ("hlt");
}

else if (string_equals(command_buffer, "cpu"))
{
    char vendor[13];
    get_cpu_vendor(vendor);

    draw_text(framebuffer, 100, cursor_y, "CPU VENDOR");
    cursor_y += 7;

    draw_text(framebuffer, 100, cursor_y, vendor);
    cursor_y += 14;
}
else if (string_equals(command_buffer, "memory"))
{
    uint64_t usable_bytes = 0;

    if (memmap_request.response != NULL)
    {
        for (uint64_t i = 0;
             i < memmap_request.response->entry_count;
             i++)
        {
            struct limine_memmap_entry *entry =
                memmap_request.response->entries[i];

            if (entry->type == LIMINE_MEMMAP_USABLE)
                usable_bytes += entry->length;
        }
    }

    char ram_text[21];
    uint_to_string(usable_bytes / (1024 * 1024), ram_text);

    draw_text(framebuffer, 100, cursor_y, "USABLE RAM MB");
    cursor_y += 7;

    draw_text(framebuffer, 100, cursor_y, ram_text);
    cursor_y += 14;
}

else if (starts_with(command_buffer, "touch "))
{
    if (ramfs_create(command_buffer + 6))
        draw_text(framebuffer, 100, cursor_y, "FILE CREATED");
    else
        draw_text(framebuffer, 100, cursor_y, "CREATE FAILED");

    cursor_y += 14;
}
else if (string_equals(command_buffer, "ls"))
{
    bool found = false;

    for (size_t i = 0; i < RAMFS_MAX_FILES; i++) {
        const char *name = ramfs_name(i);

        if (name) {
            draw_text(framebuffer, 100, cursor_y, name);
            cursor_y += 14;
            found = true;
        }
    }

    if (!found) {
        draw_text(framebuffer, 100, cursor_y, "NO FILES");
        cursor_y += 14;
    }
}
else if (starts_with(command_buffer, "cat "))
{
    const char *content = ramfs_read(command_buffer + 4);

    if (content)
        draw_text(framebuffer, 100, cursor_y, content);
    else
        draw_text(framebuffer, 100, cursor_y, "FILE NOT FOUND");

    cursor_y += 14;
}
else if (starts_with(command_buffer, "rm "))
{
    if (ramfs_delete(command_buffer + 3))
        draw_text(framebuffer, 100, cursor_y, "FILE DELETED");
    else
        draw_text(framebuffer, 100, cursor_y, "FILE NOT FOUND");

    cursor_y += 14;
}
else if (starts_with(command_buffer, "write "))
{
    char *name = command_buffer + 6;
    char *content = name;

    while (*content && *content != ' ')
        content++;

    if (*content == '\0') {
        draw_text(framebuffer, 100, cursor_y, "USAGE WRITE NAME TEXT");
    }
    else {
        *content = '\0';
        content++;

        if (ramfs_write(name, content))
            draw_text(framebuffer, 100, cursor_y, "FILE UPDATED");
        else
            draw_text(framebuffer, 100, cursor_y, "WRITE FAILED");
    }

    cursor_y += 14;
}
else if (string_equals(command_buffer, "ticks"))
{
    char buffer[21];
    uint_to_string(timer_ticks(), buffer);

    draw_text(framebuffer, 100, cursor_y, "TIMER TICKS");
    cursor_y += 7;
    draw_text(framebuffer, 100, cursor_y, buffer);
    cursor_y += 14;
}
else if (string_equals(command_buffer, "uptime"))
{
    char buffer[21];
    uint_to_string(timer_uptime(), buffer);

    draw_text(framebuffer, 100, cursor_y, "UPTIME SECONDS");
    cursor_y += 7;
    draw_text(framebuffer, 100, cursor_y, buffer);
    cursor_y += 14;
}

else if (command_length > 0)
{
    draw_text(framebuffer, 100, cursor_y, "UNKNOWN COMMAND");
    cursor_y += 14;
}

    command_length = 0;
    cursor_x = 100;
}
    else if (character == '\b')
    {
        if (command_length == 0)
            return;

        command_length--;
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
                    BACKGROUND_COLOR;
        }
    }
    else
    {
        if (command_length >= 127)
            return;

        command_buffer[command_length++] = character;
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
static volatile uint64_t counter_a = 0;
static volatile uint64_t counter_b = 0;

static void task_a(void)
{
    while (1) {
        counter_a++;
        asm volatile ("pause");
    }
}

static void task_b(void)
{
    while (1) {
        counter_b++;
        asm volatile ("pause");
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
    heap_init();
    ramfs_init();
    void *test1 = kmalloc(128);
    void *test2 = kmalloc(256);

    if (test1 == NULL || test2 == NULL)
        hcf();

    kfree(test1);
    kfree(test2);

    scheduler_init();

    int pid_a = task_create(task_a);
    int pid_b = task_create(task_b);

    if (pid_a < 0 || pid_b < 0)
        hcf();

idt_init();
    pic_remap(hhdm_request.response->offset);
    timer_init();
    

    struct limine_framebuffer *framebuffer =
        framebuffer_request.response->framebuffers[0];

    uint32_t *pixels = framebuffer->address;

    for (uint64_t y = 0; y < framebuffer->height; y++)
    {
        for (uint64_t x = 0; x < framebuffer->width; x++)
        {
            pixels[y * framebuffer->pitch / 4 + x] = 0x0030104F;
        }
    }
    draw_text(framebuffer, 100, 100, "HELLO FROM MYOS");
    if (scheduler_task_count() == 2 &&
    scheduler_get_task(0)->state == TASK_READY &&
    scheduler_get_task(1)->state == TASK_READY)
{
    draw_text(framebuffer, 100, 110,
              "TASK RECORDS READY");
}

    asm volatile ("sti");
    hcf();
}