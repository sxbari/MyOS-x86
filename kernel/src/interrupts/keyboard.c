#include <stdint.h>
#include <stdbool.h>

extern void keyboard_put_char(char character);
extern void pic_eoi(void);

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

void keyboard_handler(void)
{
    uint8_t scancode = inb(0x60);
    static bool shift_pressed;

    if (scancode == 0x2A || scancode == 0x36)
    {
        shift_pressed = true;
    }
    else if (scancode == 0xAA || scancode == 0xB6)
    {
        shift_pressed = false;
    }
    else if ((scancode & 0x80) == 0)
    {
        char character = 0;

        if (scancode >= 0x10 && scancode <= 0x19)
        {
            static const char row[] = "qwertyuiop";
            character = row[scancode - 0x10];
        }
        else if (scancode >= 0x1E && scancode <= 0x26)
        {
            static const char row[] = "asdfghjkl";
            character = row[scancode - 0x1E];
        }
        else if (scancode >= 0x2C && scancode <= 0x32)
        {
            static const char row[] = "zxcvbnm";
            character = row[scancode - 0x2C];
        }
        else if (scancode >= 0x02 && scancode <= 0x0A)
        {
            character = '1' + (scancode - 0x02);
        }
        else if (scancode == 0x0B)
        {
            character = '0';
        }
        else if (scancode == 0x39)
        {
            character = ' ';
        }
        else if (scancode == 0x1C)
        {
            character = '\n';
        }
        else if (scancode == 0x0E)
        {
            character = '\b';
        }

        if (character != 0)
        {
            if (shift_pressed && character >= 'a' && character <= 'z')
                character -= 'a' - 'A';

            keyboard_put_char(character);
        }
    }

    pic_eoi();
}