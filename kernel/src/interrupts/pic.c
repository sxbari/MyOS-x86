#include <stdint.h>

#include <stdbool.h>

static inline void outb(uint16_t port, uint8_t value)
{
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint64_t rdmsr(uint32_t msr)
{
    uint32_t low;
    uint32_t high;

    asm volatile (
        "rdmsr"
        : "=a"(low), "=d"(high)
        : "c"(msr)
    );

    return ((uint64_t)high << 32) | low;
}

static inline void wrmsr(uint32_t msr, uint64_t value)
{
    uint32_t low = value;
    uint32_t high = value >> 32;

    asm volatile (
        "wrmsr"
        :
        : "c"(msr), "a"(low), "d"(high)
    );
}

static volatile uint32_t *local_apic_registers;
static bool x2apic_enabled;

static volatile uint32_t *map_local_apic(uint64_t hhdm_offset, uint64_t physical)
{
    uint64_t cr3;
    asm volatile ("mov %%cr3, %0" : "=r"(cr3));

    volatile uint64_t *pml4 =
        (volatile uint64_t *)(hhdm_offset + (cr3 & 0x000FFFFFFFFFF000ULL));
    uint64_t pml4_entry = pml4[511];
    if ((pml4_entry & 1) == 0)
        return 0;

    volatile uint64_t *pdpt = (volatile uint64_t *)(
        hhdm_offset + (pml4_entry & 0x000FFFFFFFFFF000ULL));
    uint64_t pdpt_entry = pdpt[510];
    if ((pdpt_entry & 1) == 0)
        return 0;

    volatile uint64_t *page_directory = (volatile uint64_t *)(
        hhdm_offset + (pdpt_entry & 0x000FFFFFFFFFF000ULL));

    for (uint64_t index = 1; index < 512; index++)
    {
        if ((page_directory[index] & 1) != 0)
            continue;

        uint64_t virtual_base = 0xFFFFFFFF80000000ULL + (index << 21);
        uint64_t physical_base = physical & ~0x1FFFFFULL;
        page_directory[index] = physical_base | 0x9B;
        asm volatile ("invlpg (%0)" : : "r"(virtual_base) : "memory");
        return (volatile uint32_t *)virtual_base;
    }

    return 0;
}

void pic_remap(uint64_t hhdm_offset)
{
    // Start initialization
    outb(0x20, 0x11);
    outb(0xA0, 0x11);

    // Set interrupt vectors
    outb(0x21, 0x20);
    outb(0xA1, 0x28);

    // Tell PICs how they are connected
    outb(0x21, 0x04);
    outb(0xA1, 0x02);

    // Set 8086 mode
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    // Mask all interrupts for now
    outb(0x21, 0xFD);
    outb(0xA1, 0xFF);

    uint64_t apic_base = rdmsr(0x1B);
    if ((apic_base & (1ULL << 11)) == 0)
    {
        apic_base |= 1ULL << 11;
        wrmsr(0x1B, apic_base);
    }

    x2apic_enabled = (apic_base & (1ULL << 10)) != 0;

    uint32_t lint0;
    if (x2apic_enabled)
    {
        lint0 = rdmsr(0x835);
    }
    else
    {
        uint64_t apic_physical = apic_base & 0x0000000FFFFFF000ULL;
        local_apic_registers = map_local_apic(hhdm_offset, apic_physical);
        if (local_apic_registers == 0)
            return;
        lint0 = local_apic_registers[0x350 / 4];
    }

    lint0 = (lint0 & ~((7U << 8) | (1U << 16))) | (7U << 8);

    if (x2apic_enabled)
    {
        wrmsr(0x835, lint0);
        uint64_t spurious = rdmsr(0x80F);
        wrmsr(0x80F, spurious | (1U << 8) | 0xFF);
    }
    else
    {
        local_apic_registers[0x350 / 4] = lint0;
        local_apic_registers[0x0F0 / 4] |= (1U << 8) | 0xFF;
    }
}

void pic_eoi(void)
{
    if (x2apic_enabled)
        wrmsr(0x80B, 0);
    else if (local_apic_registers != 0)
        local_apic_registers[0x0B0 / 4] = 0;

    outb(0x20, 0x20);
}