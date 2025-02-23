// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/syslog/debug.hpp"
#include "mos/syslog/formatter.hpp"

#include <mos/string_view.hpp>
#include <mos/type_utils.hpp>

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
    struct SyslogStream
    {
        explicit SyslogStream(DebugFeature feature, LogLevel level);

        template<typename T>
        explicit SyslogStream(DebugFeature feature, LogLevel level, const T &value) : SyslogStream(feature, level)
        {
            if (should_print)
                *this << value;
        }

        // disable copy constructor
        SyslogStream(const SyslogStream &) = delete;
        SyslogStream &operator=(const SyslogStream &) = delete;

        // also disable move constructor
        SyslogStream(SyslogStream &&) = delete;
        SyslogStream &operator=(SyslogStream &&) = delete;

        ~SyslogStream();

        template<typename T>
        requires(std::is_integral_v<T>) inline SyslogStream &operator<<(T value)
        {
            if (!should_print)
                return *this;

            pos += snprintf(fmtbuffer + pos, MOS_PRINTK_BUFFER_SIZE, "%lld", (long long) value);
            return *this;
        }

        inline SyslogStream &operator<<(char c)
        {
            if (!should_print)
                return *this;

            pos += snprintf(fmtbuffer + pos, MOS_PRINTK_BUFFER_SIZE, "%c", c);
            return *this;
        }

        inline SyslogStream &operator<<(const char *str)
        {
            if (!should_print)
                return *this;

            pos += snprintf(fmtbuffer + pos, MOS_PRINTK_BUFFER_SIZE, "%s", str);
            return *this;
        }

        inline SyslogStream &operator<<(mos::string_view sv)
        {
            if (!should_print)
                return *this;

            pos += snprintf(fmtbuffer + pos, MOS_PRINTK_BUFFER_SIZE, "%.*s", (int) sv.size(), sv.data());
            return *this;
        }

      private:
        char fmtbuffer[MOS_PRINTK_BUFFER_SIZE] = { 0 };
        size_t pos = 0;

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
        SyslogStream operator<<(const T &value) const
        {
            // copy-elision
            return SyslogStream(feature, level, value);
        }
    };

    template<typename M, typename... Args>
    struct Preformatted
    {
        const std::tuple<Args...> targs;

        explicit Preformatted(M, Args... args) : targs(args...) {};

        friend SyslogStream &operator<<(SyslogStream &stream, Preformatted fmt)
        {
            mos::FormatImpl::print_m<M>(stream, fmt.targs);
            return stream;
        }
    };

} // namespace mos

#define DefineLogStream(name, level)                                                                                                                                     \
    constexpr auto inline m##name = mos::LoggingDescriptor<_none, LogLevel::level>();                                                                                    \
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
