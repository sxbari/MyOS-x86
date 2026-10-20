#ifndef MYOS_MOUSE_H
#define MYOS_MOUSE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int16_t dx;
    int16_t dy;
    uint8_t buttons;
} mouse_event_t;

bool mouse_init(void);
void mouse_irq_handler(void);
bool mouse_poll(mouse_event_t *event);

#endif