#include <stdint.h>
#include "timer.h"

#define PIT_FREQUENCY 100
#define PIT_BASE_FREQUENCY 1193182

static volatile uint64_t ticks = 0;

static inline void outb(uint16_t port, uint8_t value)
{
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    asm volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void timer_init(void)
{
    uint16_t divisor = PIT_BASE_FREQUENCY / PIT_FREQUENCY;

    // Configure PIT channel 0, mode 3.
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, divisor >> 8);

    // Enable IRQ0 while keeping IRQ1 enabled.
    uint8_t mask = inb(0x21);
    outb(0x21, mask & (uint8_t)~1U);
}

void timer_handler(void)
{
    ticks++;
}

uint64_t timer_ticks(void)
{
    return ticks;
}

uint64_t timer_uptime(void)
{
    return ticks / PIT_FREQUENCY;
}