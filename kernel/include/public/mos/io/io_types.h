// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

typedef enum
{
    IO_SEEK_SET = 0,     // set to the given value
    IO_SEEK_CURRENT = 1, // set to the current offset + the given value
    IO_SEEK_END = 2,     // set to the end of the file + the given value
} io_seek_whence_t;
