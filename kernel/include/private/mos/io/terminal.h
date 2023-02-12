// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/io/io.h"

typedef struct console_t console_t;

typedef enum
{
    TERM_TYPE_CONSOLE,
    TERM_TYPE_PIPE,
} term_type;

typedef struct _terminal
{
    term_type type;
    io_t io;
    union
    {
        console_t *console;
        struct
        {
            io_t *read;
            io_t *write;
        } pipe;
    };
} terminal_t;

terminal_t *terminal_create_console(console_t *console);
terminal_t *terminal_create_pipe(io_t *read, io_t *write);
