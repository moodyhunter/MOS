// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

/**
 * @defgroup down_stack libs.DownStack
 * @ingroup libs
 * @brief A stack that grows down.
 * @{
 */
typedef struct _downwards_stack_t
{
    uintptr_t top;
    uintptr_t head;
    size_t capacity;
} downwards_stack_t;

void stack_init(downwards_stack_t *stack, void *mem_region_bottom, size_t size);
void stack_deinit(downwards_stack_t *stack);

void *stack_grow(downwards_stack_t *stack, size_t size);
void stack_push(downwards_stack_t *stack, const void *data, size_t size);

// ! WARN: Caller must ensure the data is at least size bytes long
void stack_pop(downwards_stack_t *stack, size_t size, void *data);

/** @} */
