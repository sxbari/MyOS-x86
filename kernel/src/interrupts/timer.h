#ifndef MYOS_TIMER_H
#define MYOS_TIMER_H

#include <stdint.h>

void timer_init(void);
void timer_handler(void);
uint64_t timer_ticks(void);
uint64_t timer_uptime(void);

#endif