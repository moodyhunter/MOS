// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <ansi_colors.h>
#include <mos/io/io.hpp>
#include <mos/lib/structures/list.hpp>
#include <mos/lib/structures/ring_buffer.hpp>
#include <mos/lib/sync/spinlock.hpp>
#include <mos/mos_global.h>
#include <mos/tasks/wait.hpp>
#include <mos/types.hpp>

typedef enum
{
    CONSOLE_CAP_COLOR = 1 << 0,
    CONSOLE_CAP_CLEAR = 1 << 1,
    CONSOLE_CAP_GET_SIZE = 1 << 2,
    CONSOLE_CAP_CURSOR_HIDE = 1 << 3,
    CONSOLE_CAP_CURSOR_MOVE = 1 << 4,
    CONSOLE_CAP_READ = 1 << 6, ///< console supports read
} console_caps;

MOS_ENUM_OPERATORS(console_caps)

template<size_t buf_size>
struct Buffer
{
    u8 buf[buf_size] __aligned(buf_size) = { 0 };
    const size_t size = buf_size;
};

struct Console // : public io_t
{
    as_linked_list;
    io_t io;
    const char *name = "<unnamed>";
    console_caps caps;
    waitlist_t waitlist; // waitlist for read

    template<size_t buf_size>
    Console(const char *name, console_caps caps, Buffer<buf_size> *read_buf, standard_color_t default_fg, standard_color_t default_bg)
        : name(name), caps(caps), fg(default_fg), bg(default_bg), default_fg(default_fg), default_bg(default_bg)
    {
        reader.buf = read_buf->buf;
        reader.size = read_buf->size;
        waitlist_init(&waitlist);
    }

    virtual ~Console() = default;

    struct
    {
        spinlock_t lock;
        ring_buffer_pos_t pos;
        u8 *buf = nullptr;
        size_t size = 0;
    } reader;

    struct
    {
        spinlock_t lock;
    } writer;

    standard_color_t fg, bg;
    standard_color_t default_fg = White, default_bg = Black;

  public:
    size_t write(const char *data, size_t size)
    {
        spinlock_acquire(&writer.lock);
        size_t ret = do_write(data, size);
        spinlock_release(&writer.lock);
        return ret;
    }

    size_t write_color(const char *data, size_t size, standard_color_t fg, standard_color_t bg)
    {
        spinlock_acquire(&writer.lock);
        if (caps & CONSOLE_CAP_COLOR)
        {
            if (this->fg != fg || this->bg != bg)
            {
                set_color(fg, bg);
                this->fg = fg;
                this->bg = bg;
            }
        }

        size_t ret = do_write(data, size);
        spinlock_release(&writer.lock);
        return ret;
    }

    void putc(u8 c);

  public:
    virtual bool extra_setup()
    {
        return true;
    }

    virtual bool get_size(u32 *width, u32 *height) = 0;
    virtual bool set_color(standard_color_t fg, standard_color_t bg) = 0;
    virtual bool clear() = 0;

  public: // TODO: make protected
    virtual size_t do_write(const char *data, size_t size) = 0;
};

extern list_head consoles;

void console_register(Console *con);
Console *console_get(const char *name);
Console *console_get_by_prefix(const char *prefix);
