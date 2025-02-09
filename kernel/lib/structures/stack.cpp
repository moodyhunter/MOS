// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/lib/structures/stack.hpp>
#include <mos/moslib_global.hpp>
#include <mos_stdio.hpp>
#include <mos_string.hpp>

void stack_init(downwards_stack_t *stack, void *mem_region_bottom, size_t size)
{
    // the stack grows downwards
    ptr_t stack_top = (ptr_t) mem_region_bottom + size;
    stack->top = stack_top;
    stack->head = stack_top;
    stack->capacity = size;
}

void stack_deinit(downwards_stack_t *stack)
{
    memset(stack, 0, sizeof(downwards_stack_t));
}

void *stack_grow(downwards_stack_t *stack, size_t size)
{
    // high memory | top -----> head -----> top - capacity | low memory
    if (unlikely(stack->head - (stack->top - stack->capacity) < size))
        mos_panic("stack overflow on stack %p, attempted to push %zu bytes", (void *) stack, size);

    stack->head = stack->head - size;
    return (void *) stack->head;
}

void *stack_push(downwards_stack_t *stack, const void *data, size_t size)
{
    // high memory | top -----> head -----> top - capacity | low memory
    if (unlikely(stack->head - (stack->top - stack->capacity) < size))
        mos_panic("stack overflow on stack %p, attempted to push %zu bytes", (void *) stack, size);

    stack->head = stack->head - size;
    memcpy((void *) stack->head, data, size);
    return (void *) stack->head;
}

void stack_pop(downwards_stack_t *stack, size_t size, void *data)
{
    // high memory | top -----> head -----> top - capacity | low memory
    if (unlikely(stack->head - stack->top < size))
        mos_panic("stack underflow on stack %p, attempted to pop %zu bytes", (void *) stack, size);

    if (data != NULL)
        memcpy(data, (void *) stack->head, size);
    stack->head = stack->head + size;
}
