// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

typedef enum
{
    HEAP_GET_BASE,
    HEAP_GET_TOP,
    HEAP_SET_TOP,
    HEAP_GET_SIZE,
    HEAP_GROW_PAGES,
} heap_control_op;
