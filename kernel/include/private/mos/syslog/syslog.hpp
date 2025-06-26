// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/syslog/debug.hpp"
#include "mos/syslog/formatter.hpp"

#include <array>
#include <mos/refcount.hpp>
#include <mos/string_view.hpp>
#include <mos/type_utils.hpp>
#include <type_traits>

enum class LogLevel
{
    FATAL = 6,
    EMERG = 5,
    WARN = 4,
    EMPH = 3,
    INFO = 2,
    INFO2 = 1,
    UNSET = 0,
};

extern struct Console *printk_console;
extern "C" int snprintf(char *__restrict str, size_t size, const char *__restrict format, ...);
extern "C" void lprintk(LogLevel loglevel, const char *format, ...);

namespace mos
{
    template<typename M, typename... Args>
    struct Preformatted
    {
        const std::tuple<Args...> targs;
        explicit Preformatted(M, Args... args) : targs(args...) {};
    };

    using SyslogBuffer = std::array<char, MOS_PRINTK_BUFFER_SIZE>;

    struct SyslogStreamWriter : private mos::_RefCounted
    {
        ~SyslogStreamWriter();

        template<typename T>
        requires std::is_integral_v<T> inline SyslogStreamWriter &operator<<(T value)
        {
            if (!should_print)
                return *this;

            pos += snprintf(&fmtbuffer[pos], MOS_PRINTK_BUFFER_SIZE, "%lld", (long long) value);
            return *this;
        }

        template<typename T>
        requires std::is_void_v<T> inline SyslogStreamWriter &operator<<(T *ptr)
        {
            if (!should_print)
                return *this;

            pos += snprintf(&fmtbuffer[pos], MOS_PRINTK_BUFFER_SIZE, "%p", ptr);
            return *this;
        }

        template<typename E>
        requires(std::is_enum_v<E>) inline SyslogStreamWriter &operator<<(E value)
        {
            if (!should_print)
                return *this;

            pos += snprintf(&fmtbuffer[pos], MOS_PRINTK_BUFFER_SIZE, "%d", (int) value);
            return *this;
        }

        inline SyslogStreamWriter &operator<<(char c)
        {
            if (!should_print)
                return *this;

            pos += snprintf(&fmtbuffer[pos], MOS_PRINTK_BUFFER_SIZE, "%c", c);
            return *this;
        }

        inline SyslogStreamWriter &operator<<(const char *str)
        {
            if (!should_print)
                return *this;

            pos += snprintf(&fmtbuffer[pos], MOS_PRINTK_BUFFER_SIZE, "%s", str);
            return *this;
        }

        inline SyslogStreamWriter &operator<<(mos::string_view sv)
        {
            if (!should_print)
                return *this;

            pos += snprintf(&fmtbuffer[pos], MOS_PRINTK_BUFFER_SIZE, "%.*s", (int) sv.size(), sv.data());
            return *this;
        }

        template<typename M, typename... Args>
        inline SyslogStreamWriter operator<<(const Preformatted<M, Args...> &fmt)
        {
            mos::FormatImpl::print_m<M>(*this, fmt.targs);
            return *this;
        }

      private:
        explicit SyslogStreamWriter(DebugFeature feature, LogLevel level, _RCCore *rcCore, SyslogBuffer &fmtbuffer, size_t &pos);

        template<DebugFeature feature, LogLevel level>
        friend struct LoggingDescriptor;

      private:
        SyslogBuffer &fmtbuffer;
        size_t &pos;

      private:
        const u64 timestamp;
        const DebugFeature feature;
        const LogLevel level;
        const bool should_print;
    };

    template<DebugFeature feature, LogLevel level>
    struct LoggingDescriptor
    {
        static constexpr auto feature_value = feature;
        static constexpr auto level_value = level;

        template<typename T>
        SyslogStreamWriter operator<<(const T &value) const
        {
            // copy-elision
            fmtBuffer[0] = '\n';
            fmtBuffer[1] = '\0';
            pos = 0;
            return SyslogStreamWriter(feature, level, &RefCounter, fmtBuffer, pos) << value;
        }

      private:
        mutable SyslogBuffer fmtBuffer{};
        mutable size_t pos = 0;
        mutable _RCCore RefCounter{};
    };
} // namespace mos

#define DefineLogStream(name, level)                                                                                                                                     \
    extern const mos::LoggingDescriptor<_none, LogLevel::level> m##name;                                                                                                 \
    template<DebugFeature feat>                                                                                                                                          \
    constexpr auto inline d##name = mos::LoggingDescriptor<feat, LogLevel::level>()

DefineLogStream(Info2, INFO2);
DefineLogStream(Info, INFO);
DefineLogStream(Emph, EMPH);
DefineLogStream(Warn, WARN);
DefineLogStream(Emerg, EMERG);
DefineLogStream(Fatal, FATAL);
DefineLogStream(Cont, UNSET);
#undef DefineLogStream

#define f(_fmt)        formatted_type(_fmt "")
#define fmt(_fmt, ...) mos::Preformatted(formatted_type(_fmt ""), ##__VA_ARGS__)

long do_syslog(LogLevel level, const char *file, const char *func, int line, const debug_info_entry *feat, const char *fmt, ...);
