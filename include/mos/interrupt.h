// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/containers.h"

typedef struct
{
    as_linked_list;
    void (*handler)(u32 irq);
} irq_handler_descriptor_t;

// void install_irq_handler(u32 irq, irq_handler_descriptor_t *handler);
