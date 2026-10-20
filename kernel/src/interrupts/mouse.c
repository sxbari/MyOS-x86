#include "mouse.h"

#define PS2_DATA   0x60
#define PS2_STATUS 0x64
#define PS2_CMD    0x64

#define MOUSE_QUEUE_SIZE 256
#define PS2_WAIT_LIMIT 1000000U

extern void pic_mouse_eoi(void);

static volatile uint32_t queue_head;
static volatile uint32_t queue_tail;

static mouse_event_t queue[MOUSE_QUEUE_SIZE];

static uint8_t packet[3];
static uint8_t packet_index;

/* =============== PORT I/O =============== */

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;

    asm volatile (
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

static inline void outb(uint16_t port, uint8_t value)
{
    asm volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

/* =============== PS/2 WAITING =============== */

static bool wait_input_empty(void)
{
    for (uint32_t i = 0; i < PS2_WAIT_LIMIT; i++) {
        if (!(inb(PS2_STATUS) & 0x02))
            return true;

        asm volatile ("pause");
    }

    return false;
}

static bool wait_output_full(void)
{
    for (uint32_t i = 0; i < PS2_WAIT_LIMIT; i++) {
        if (inb(PS2_STATUS) & 0x01)
            return true;

        asm volatile ("pause");
    }

    return false;
}

static bool write_controller(uint8_t value)
{
    if (!wait_input_empty())
        return false;

    outb(PS2_CMD, value);
    return true;
}

static bool write_data(uint8_t value)
{
    if (!wait_input_empty())
        return false;

    outb(PS2_DATA, value);
    return true;
}

/* =============== MOUSE COMMANDS =============== */

static bool mouse_send(uint8_t value)
{
    // Route next data byte to mouse.
    if (!write_controller(0xD4))
        return false;

    if (!write_data(value))
        return false;

    if (!wait_output_full())
        return false;

    // Mouse should acknowledge with 0xFA.
    return inb(PS2_DATA) == 0xFA;
}

/* =============== INITIALIZATION =============== */

bool mouse_init(void)
{
    queue_head = 0;
    queue_tail = 0;
    packet_index = 0;

    // Enable second PS/2 port.
    if (!write_controller(0xA8))
        return false;

    // Read controller configuration.
    if (!write_controller(0x20))
        return false;

    if (!wait_output_full())
        return false;

    uint8_t config = inb(PS2_DATA);

    // Enable IRQ12.
    config |= 0x02;

    // Enable mouse clock.
    config &= (uint8_t)~0x20;

    // Write updated configuration.
    if (!write_controller(0x60))
        return false;

    if (!write_data(config))
        return false;

    // Standard 3-byte PS/2 mouse.
    if (!mouse_send(0xF6))
        return false;

    // Enable mouse movement packets.
    if (!mouse_send(0xF4))
        return false;

    return true;
}

/* =============== QUEUE PACKETS =============== */

static void enqueue_packet(void)
{
    // Discard packets with movement overflow.
    if (packet[0] & 0xC0)
        return;

    uint32_t next =
        (queue_head + 1) % MOUSE_QUEUE_SIZE;

    // Queue full.
    if (next == queue_tail)
        return;

    uint16_t x = packet[1];
    uint16_t y = packet[2];

    // Sign extension using first-byte flags.
    if (packet[0] & 0x10)
        x |= 0xFF00;

    if (packet[0] & 0x20)
        y |= 0xFF00;

    queue[queue_head].dx = (int16_t)x;
    queue[queue_head].dy = (int16_t)y;
    queue[queue_head].buttons = packet[0] & 0x07;

    asm volatile ("" ::: "memory");

    queue_head = next;
}

/* =============== IRQ12 HANDLER =============== */

void mouse_irq_handler(void)
{
    uint8_t status;

    while (((status = inb(PS2_STATUS)) & 0x21) == 0x21) {
        uint8_t byte = inb(PS2_DATA);

        if (packet_index == 0) {
            // First mouse byte has bit 3 set.
            if (byte & 0x08) {
                packet[0] = byte;
                packet_index = 1;
            }
        }
        else {
            packet[packet_index++] = byte;

            if (packet_index == 3) {
                enqueue_packet();
                packet_index = 0;
            }
        }
    }

    pic_mouse_eoi();
}

/* =============== READ FROM MAIN LOOP =============== */

bool mouse_poll(mouse_event_t *event)
{
    if (!event || queue_tail == queue_head)
        return false;

    *event = queue[queue_tail];

    asm volatile ("" ::: "memory");

    queue_tail =
        (queue_tail + 1) % MOUSE_QUEUE_SIZE;

    return true;
}