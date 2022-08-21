// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/stack.h"

#include "lib/string.h"
#include "mos/mm/kmalloc.h"
#include "mos/printk.h"

void stack_init(downwards_stack_t *stack, void *mem_region_bottom, size_t size)
{
    // the stack grows downwards, so the base of the stack is the bottom of the memory region
    void *stack_base = (void *) ((uintptr_t) mem_region_bottom + size);

    *((void **) &stack->base) = stack_base;
    stack->head = stack_base;
    stack->capacity = size;
}

void stack_deinit(downwards_stack_t *stack)
{
    memset(stack, 0, sizeof(downwards_stack_t));
}

void *stack_grow(downwards_stack_t *stack, size_t size)
{
    // high memory | base -----> head -----> base - capacity | low memory
    if (unlikely((uintptr_t) stack->head - ((uintptr_t) stack->base - stack->capacity) < size))
    {
        mos_warn("stack overflow on stack %p, attempted to push %zu bytes", (void *) stack, size);
        return NULL;
    }

    stack->head = (void *) ((uintptr_t) stack->head - size);
    return stack->head;
}

void stack_push(downwards_stack_t *stack, void *data, size_t size)
{
    // high memory | base -----> head -----> base - capacity | low memory
    if (unlikely((uintptr_t) stack->head - ((uintptr_t) stack->base - stack->capacity) < size))
    {
        mos_warn("stack overflow on stack %p, attempted to push %zu bytes", (void *) stack, size);
        return;
    }

    stack->head = (void *) ((uintptr_t) stack->head - size);
    memcpy(stack->head, data, size);
}

void stack_pop(downwards_stack_t *stack, void *data, size_t size)
{
    // high memory | base -----> head -----> base - capacity | low memory
    if (unlikely((uintptr_t) stack->head - (uintptr_t) stack->base < size))
    {
        mos_warn("stack underflow on stack %p, attempted to pop %zu bytes", (void *) stack, size);
        return;
    }

    memcpy(data, stack->head, size);
    // memset(stack->head, 0, size);
    stack->head = (void *) ((uintptr_t) stack->head + size);
}
