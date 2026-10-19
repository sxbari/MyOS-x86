
#include "heap.h"
#include <stdint.h>
#include <stdbool.h>

#define HEAP_SIZE (1024 * 1024)

typedef struct block {
    size_t size;
    bool free;
    struct block *next;
} __attribute__((aligned(16))) block_t;

static uint8_t heap_area[HEAP_SIZE]
    __attribute__((aligned(16)));

static block_t *head;

static size_t align16(size_t size)
{
    return (size + 15) & ~(size_t)15;
}

void heap_init(void)
{
    head = (block_t *)heap_area;
    head->size = HEAP_SIZE - sizeof(block_t);
    head->free = true;
    head->next = NULL;
}

void *kmalloc(size_t size)
{
    if (size == 0 || size > HEAP_SIZE - sizeof(block_t))
        return NULL;

    size = align16(size);

    for (block_t *block = head; block; block = block->next)
    {
        if (!block->free || block->size < size)
            continue;

        if (block->size >= size + sizeof(block_t) + 16)
        {
            block_t *next = (block_t *)
                ((uint8_t *)(block + 1) + size);

            next->size = block->size - size - sizeof(block_t);
            next->free = true;
            next->next = block->next;

            block->next = next;
            block->size = size;
        }

        block->free = false;
        return block + 1;
    }

    return NULL;
}

void kfree(void *ptr)
{
    if (!ptr)
        return;

    for (block_t *block = head; block; block = block->next)
    {
        if ((void *)(block + 1) == ptr)
        {
            if (block->free)
                return;

            block->free = true;

            while (block->next && block->next->free)
            {
                block->size += sizeof(block_t) + block->next->size;
                block->next = block->next->next;
            }

            // Coalesce with preceding free block.
            for (block_t *prev = head; prev && prev != block;
                 prev = prev->next)
            {
                if (prev->next == block && prev->free)
                {
                    prev->size += sizeof(block_t) + block->size;
                    prev->next = block->next;
                    break;
                }
            }
            return;
        }
    }
}

size_t heap_used(void)
{
    size_t used = 0;

    for (block_t *block = head; block; block = block->next)
        if (!block->free)
            used += block->size;

    return used;
}

size_t heap_capacity(void)
{
    return HEAP_SIZE;
}
