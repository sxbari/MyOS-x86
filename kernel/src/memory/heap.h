
#ifndef MYOS_HEAP_H
#define MYOS_HEAP_H

#include <stddef.h>

void heap_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);
size_t heap_used(void);
size_t heap_capacity(void);

#endif



