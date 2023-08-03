// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

typedef enum
{
    IO_SEEK_CURRENT = 1, // set to the current offset + the given value
    IO_SEEK_END = 2,     // set to the end of the file + the given value
    IO_SEEK_SET = 3,     // set to the given value
    IO_SEEK_DATA = 4,
    IO_SEEK_HOLE = 5,
} io_seek_whence_t;
