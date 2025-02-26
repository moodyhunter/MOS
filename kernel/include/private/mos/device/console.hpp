// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <ansi_colors.h>
#include <array>
#include <mos/io/io.hpp>
#include <mos/lib/structures/list.hpp>
#include <mos/lib/structures/ring_buffer.hpp>
#include <mos/lib/sync/spinlock.hpp>
#include <mos/mos_global.h>
#include <mos/tasks/wait.hpp>
#include <mos/types.hpp>

enum ConsoleCapability
{
    CONSOLE_CAP_COLOR = 1 << 0,
    CONSOLE_CAP_CLEAR = 1 << 1,
    CONSOLE_CAP_GET_SIZE = 1 << 2,
    CONSOLE_CAP_CURSOR_HIDE = 1 << 3,
    CONSOLE_CAP_CURSOR_MOVE = 1 << 4,
    CONSOLE_CAP_READ = 1 << 6, ///< console supports read
};

MOS_ENUM_FLAGS(ConsoleCapability, ConsoleCapFlags);

template<size_t buf_size>
struct Buffer
{
    u8 buf[buf_size] __aligned(buf_size) = { 0 };
    const size_t size = buf_size;
};

struct Console : public IO
{
    StandardColor fg, bg;

  public:
    template<size_t buf_size>
    Console(mos::string_view name, ConsoleCapFlags caps, Buffer<buf_size> *readBuf, StandardColor fg, StandardColor bg) : Console(name, caps, fg, bg)
    {
        this->reader.buf = readBuf->buf;
        this->reader.size = readBuf->size;
        ring_buffer_pos_init(&reader.pos, reader.size);
    }

    virtual ~Console() = default;

    void Register();

  public:
    size_t Write(const char *data, size_t size);
    size_t WriteColored(const char *data, size_t size, StandardColor fg, StandardColor bg);
    void putc(u8 c);

  private:
    virtual bool clear() = 0;
    virtual bool set_color(StandardColor fg, StandardColor bg) = 0;
    virtual size_t do_write(const char *data, size_t size) = 0;

  public:
    // IO interface
    virtual size_t on_read(void *, size_t) override;
    virtual size_t on_write(const void *, size_t) override;
    virtual void on_closed() override;

  public:
    // IO interface
    virtual mos::string name() const override;

  private:
    Console(mos::string_view name, ConsoleCapFlags caps, StandardColor default_fg, StandardColor default_bg);

  private:
    const ConsoleCapFlags caps;
    const StandardColor default_fg = White, default_bg = Black;

    struct reader
    {
        spinlock_t lock;
        u8 *buf = nullptr;
        ring_buffer_pos_t pos;
        size_t size = 0;
    } reader;

    struct
    {
        spinlock_t lock;
    } writer;

    mos::string_view conName = "<unnamed>";
    waitlist_t waitlist; // waitlist for readers
};

extern std::array<Console *, 128> consoles;

std::optional<Console *> console_get(mos::string_view name);
std::optional<Console *> console_get_by_prefix(mos::string_view name);
