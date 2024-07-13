// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.h>

/**
 * @brief Function pointer type for interrupt handlers
 * @returns true if the interrupt was handled, false otherwise, in which case the interrupt will be passed to the next handler
 */
typedef bool (*irq_serve_t)(u32 irq, void *data);

void interrupt_entry(u32 irq);

/**
 * @brief Register an interrupt handler
 *
 * @param irq The interrupt number
 * @param handler The handler function
 * @param data Data to pass to the handler
 */
void interrupt_handler_register(u32 irq, irq_serve_t handler, void *data);
