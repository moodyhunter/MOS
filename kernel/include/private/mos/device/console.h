// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <ansi_colors.h>
#include <mos/io/io.h>
#include <mos/lib/structures/list.h>
#include <mos/lib/structures/ring_buffer.h>
#include <mos/lib/sync/spinlock.h>
#include <mos/tasks/wait.h>
#include <mos/types.h>

typedef enum
{
    CONSOLE_CAP_COLOR = 1 << 0,
    CONSOLE_CAP_CLEAR = 1 << 1,
    CONSOLE_CAP_GET_SIZE = 1 << 2,
    CONSOLE_CAP_CURSOR_HIDE = 1 << 3,
    CONSOLE_CAP_CURSOR_MOVE = 1 << 4,
    CONSOLE_CAP_EXTRA_SETUP = 1 << 5, // extra setup required
    CONSOLE_CAP_READ = 1 << 6,        ///< console supports read
} console_caps;

typedef struct _console
{
    as_linked_list;
    io_t io;
    struct console_ops *ops;
    const char *name;
    console_caps caps;
    waitlist_t waitlist; // waitlist for read

    struct
    {
        spinlock_t lock;
        ring_buffer_pos_t pos;
        u8 *buf;
        size_t size;
    } read;

    struct
    {
        spinlock_t lock;
    } write;

    standard_color_t default_fg, default_bg;
} console_t;

typedef struct console_ops
{
    bool (*extra_setup)(console_t *con);

    size_t (*write)(console_t *con, const char *data, size_t size);

    bool (*get_size)(console_t *con, u32 *width, u32 *height);

    bool (*set_cursor)(console_t *con, bool show);
    bool (*move_cursor)(console_t *con, u32 x, u32 y);
    bool (*get_cursor)(console_t *con, u32 *x, u32 *y);

    // VGA standard color codes
    bool (*get_color)(console_t *con, standard_color_t *fg, standard_color_t *bg);
    bool (*set_color)(console_t *con, standard_color_t fg, standard_color_t bg);

    bool (*clear)(console_t *con);
} console_ops_t;

extern list_head consoles;
void console_register(console_t *con);
console_t *console_get(const char *name);
console_t *console_get_by_prefix(const char *prefix);

size_t console_write(console_t *con, const char *data, size_t size);
size_t console_write_color(console_t *con, const char *data, size_t size, standard_color_t fg, standard_color_t bg);

void console_putc(console_t *con, u8 c);
