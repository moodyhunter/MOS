// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/lib/sync/spinlock.hpp"
#include "mos/syslog/syslog.hpp"
#include "mos/tasks/signal.hpp"
#include "mos/tasks/thread.hpp"

#include <array>
#include <limits.h>
#include <mos/device/console.hpp>
#include <mos/io/io.hpp>
#include <mos/lib/structures/list.hpp>
#include <mos/lib/structures/ring_buffer.hpp>
#include <mos/string.hpp>
#include <mos/syslog/printk.hpp>
#include <mos/tasks/schedule.hpp>
#include <mos/tasks/wait.hpp>
#include <mos_string.hpp>

std::array<Console *, 128> consoles;
size_t console_list_size = 0;

std::optional<Console *> console_get(mos::string_view name)
{
    for (const auto &console : consoles)
    {
        if (console->name() == name)
            return console;
    }

    return std::nullopt;
}
std::optional<Console *> console_get_by_prefix(mos::string_view prefix)
{
    for (const auto &con : consoles)
    {
        if (con->name().starts_with(prefix))
            return con;
    }

    return std::nullopt;
}

Console::Console(mos::string_view name, ConsoleCapFlags caps, StandardColor default_fg, StandardColor default_bg)
    : IO(IO_READABLE | ((caps & CONSOLE_CAP_READ) ? IO_WRITABLE : IO_NONE), IO_CONSOLE), //
      fg(default_fg), bg(default_bg),                                                    //
      caps(caps),                                                                        //
      default_fg(default_fg), default_bg(default_bg),                                    //
      conName(name)                                                                      //
{
}

void Console::Register()
{
    if (printk_console == nullptr)
        printk_console = this;
    consoles[console_list_size++] = this;
}

size_t Console::WriteColored(const char *data, size_t size, StandardColor fg, StandardColor bg)
{
    spinlock_acquire(&writer.lock);
    if (caps.test(CONSOLE_CAP_COLOR))
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

size_t Console::Write(const char *data, size_t size)
{
    spinlock_acquire(&writer.lock);
    size_t ret = do_write(data, size);
    spinlock_release(&writer.lock);
    return ret;
}
size_t Console::on_read(void *data, size_t size)
{
    size_t read = 0;
    while (read == 0)
    {
        SpinLocker locker(&reader.lock);

        const bool buffer_empty = ring_buffer_pos_is_empty(&reader.pos);
        if (!buffer_empty)
        {
            read = ring_buffer_pos_pop_front(reader.buf, &reader.pos, (u8 *) data, size);
            continue;
        }

        {
            auto unlocker = locker.UnlockTemporarily();
            bool ok = reschedule_for_waitlist(&waitlist);
            if (!ok)
            {
                unlocker.discard(), locker.discard();
                pr_emerg("console: '%s' closed", conName);
                return -EIO;
            }
        }

        if (signal_has_pending())
        {
            return -ERESTARTSYS;
        }
    }

    return read;
}

size_t Console::on_write(const void *data, size_t size)
{
    SpinLocker locker(&writer.lock);
    if (caps.test(CONSOLE_CAP_COLOR))
        set_color(default_fg, default_bg);
    return do_write((const char *) data, size);
}

void Console::putc(u8 c)
{
    if (c == 0x3)
    {
        spinlock_acquire(&waitlist.lock);
        list_foreach(waitable_list_entry_t, entry, waitlist.list)
        {
            Thread *thread = thread_get(entry->waiter);
            if (thread)
                signal_send_to_thread(thread, SIGINT);
        }
        spinlock_release(&waitlist.lock);
    }

    ring_buffer_pos_push_back_byte(reader.buf, &reader.pos, c);
    waitlist_wake(&waitlist, INT_MAX);
}

mos::string Console::name() const
{
    return mos::string(conName);
}

void Console::on_closed()
{
    mInfo << "Closing console " << conName;
}
