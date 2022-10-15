// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/tasks/task_type.h"

typedef enum
{
    STDIO_TYPE_STDIN,
    STDIO_TYPE_STDOUT,
    STDIO_TYPE_STDERR,
} stdio_type;

typedef struct
{
    io_t io;
    stdio_type type;
} stdio_t;

void process_stdio_setup(process_t *process);
