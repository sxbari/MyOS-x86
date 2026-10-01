#include <stdint.h>

extern void interrupt_stub(void);
extern void keyboard_stub(void);

struct idt_entry
{
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attributes;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_pointer
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_pointer idt_ptr;

static void idt_set_gate(
    int vector,
    void (*handler)(void)
)
{
    uint64_t address = (uint64_t)handler;
    uint16_t code_selector;

    asm volatile ("mov %%cs, %0" : "=r"(code_selector));

    idt[vector].offset_low = address & 0xFFFF;
    idt[vector].selector = code_selector;
    idt[vector].ist = 0;
    idt[vector].type_attributes = 0x8E;
    idt[vector].offset_mid = (address >> 16) & 0xFFFF;
    idt[vector].offset_high = (address >> 32) & 0xFFFFFFFF;
    idt[vector].zero = 0;
}
void idt_init(void)
{

    idt_set_gate(32, interrupt_stub);
    idt_set_gate(33, keyboard_stub);
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint64_t)&idt;

    asm volatile ("lidt %0" : : "m"(idt_ptr));
}