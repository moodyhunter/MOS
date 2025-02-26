// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <limits>
#include <mos/type_utils.hpp>
#include <tuple>
#include <type_traits>

namespace mos
{
    struct Specifier
    {
        enum class FormatAlignment
        {
            _DEFAULT, ///< default, left
            LEFT,     ///< left
            RIGHT,    ///< right
            CENTER,   ///< center
        };

        enum class FormatSign
        {
            _DEFAULT,      ///< default, negative
            BOTH,          ///< both native and positive numbers are prefixed with a sign
            NEGATIVE,      ///< only negative numbers are prefixed with a sign
            LEADING_SPACE, ///< positive numbers are prefixed with a leading space
        };

        enum class WidthPrecisionOrder
        {
            _NOT_SPECIFIED,
            WIDTH,
            PRECISION,
        };

        static constexpr auto DYNAMIC = std::numeric_limits<size_t>::max();

        FormatAlignment alignment = FormatAlignment::_DEFAULT;

        size_t width = 0;
        size_t precision = 0;

        // which one comes first, width or precision?
        // this will be used at runtime to determine which one to set first
        WidthPrecisionOrder order = WidthPrecisionOrder::_NOT_SPECIFIED;

        FormatSign sign = FormatSign::_DEFAULT;
        char filler = '\0';
        bool debug = false;

        // #, alternative form. For integer specifiers (d, i, o, u, x, X), a non-zero result will have the
        // string '0x' (or '0X' for 'X' conversions) prepended to it.
        bool alternative = false;

        /**
         * @brief number of runtime arguments required to print this item.
         */
        consteval size_t n_dynamic_args() const
        {
            return (width == DYNAMIC) + (precision == DYNAMIC) + 1;
        }
    };

    struct FormatImpl
    {
      private:
        template<Specifier __spec>
        struct Formatter
        {
            static constexpr auto spec = __spec;

            template<typename TArg, Specifier _config = spec, std::enable_if_t<_config.n_dynamic_args() == 1, int> = 0>
            static constexpr auto arg_tuple() -> std::tuple<TArg>;

            template<typename TArg, Specifier _config = spec, std::enable_if_t<_config.n_dynamic_args() == 2, int> = 0>
            static constexpr auto arg_tuple() -> std::tuple<TArg, size_t>;

            template<typename TArg, Specifier _config = spec, std::enable_if_t<_config.n_dynamic_args() == 3, int> = 0>
            static constexpr auto arg_tuple() -> std::tuple<TArg, size_t, size_t>;

            template<typename TArg, typename Stream>
            inline Stream &Print(Stream &os, const decltype(arg_tuple<TArg>()) &args)
            {
                if constexpr (spec.n_dynamic_args() == 1)
                {
                    const auto &[arg] = args;
                    os << arg;
                }
                else if constexpr (spec.n_dynamic_args() == 2)
                {
                    const auto &[arg, n1] = args;
                    os << '[' << arg << " " << n1 << ']';
                }
                else if constexpr (spec.n_dynamic_args() == 3)
                {
                    const auto &[arg, n1, n2] = args;
                    os << '[' << arg << " " << n1 << " " << n2 << ']';
                }
                return os;
            }
        };

      private:
        template<Specifier orig>
        static consteval Specifier SetAlignment(Specifier::FormatAlignment alignment)
        {
            static_assert(orig.alignment == Specifier::FormatAlignment::_DEFAULT, "Alignment already set");
            Specifier config = orig;
            config.alignment = alignment;
            return config;
        }

        template<Specifier orig>
        static consteval Specifier SetWidth(size_t width)
        {
            Specifier config = orig;
            config.width = width;
            if (width == Specifier::DYNAMIC && config.order == Specifier::WidthPrecisionOrder::_NOT_SPECIFIED)
                config.order = Specifier::WidthPrecisionOrder::WIDTH;
            return config;
        }

        template<Specifier orig>
        static consteval Specifier SetPrecision(size_t precision)
        {
            Specifier config = orig;
            config.precision = precision;
            if (precision == Specifier::DYNAMIC && config.order == Specifier::WidthPrecisionOrder::_NOT_SPECIFIED)
                config.order = Specifier::WidthPrecisionOrder::PRECISION;
            return config;
        }

        template<Specifier orig>
        static consteval Specifier SetSign(Specifier::FormatSign sign)
        {
            static_assert(orig.sign == Specifier::FormatSign::_DEFAULT, "Sign already set");
            Specifier config = orig;
            config.sign = sign;
            return config;
        }

        template<Specifier orig>
        static consteval Specifier SetFiller(char fill)
        {
            static_assert(orig.filler == '\0', "Fill already set");
            Specifier config = orig;
            config.filler = fill;
            return config;
        }

        template<Specifier orig>
        static consteval Specifier SetDebugFormat(bool debug_format)
        {
            static_assert(orig.debug == false, "Debug format already set");
            Specifier config = orig;
            config.debug = debug_format;
            return config;
        }

        template<Specifier orig>
        static consteval Specifier SetAlternativeForm(bool alternative_form)
        {
            static_assert(orig.alternative == false, "Alternative form already set");
            Specifier config = orig;
            config.alternative = alternative_form;
            return config;
        }

      private:
        consteval static auto tuple_prepend(auto a, auto b = std::tuple())
        {
            return std::tuple_cat(std::make_tuple(a), b);
        }

        // clang-format off
        template<typename T>    struct is_formatter : std::false_type {};
        template<Specifier c>   struct is_formatter<Formatter<c>> : std::true_type {};
        template<typename T>    static consteval bool is_formatter_v() { return is_formatter<T>::value; };
        // clang-format on

        template<typename T>
        static consteval size_t n_dynamic_args()
        {
            if constexpr (is_formatter_v<T>())
                return T::spec.n_dynamic_args();
            else
                return 0;
        }

        template<typename... Type>
        static consteval size_t n_args_expected(std::tuple<>)
        {
            return 0;
        }

        template<typename... Type>
        static consteval size_t n_args_expected(std::tuple<Type...>)
        {
            return ((is_formatter_v<Type>() ? n_dynamic_args<Type>() : 0) + ...);
        }

        template<string_literal view, size_t I, size_t End, size_t Result = 0>
        static consteval size_t __do_parse_int()
        {
            if constexpr (view[I] < '0' || view[I] > '9' || I == view.strlen || I == End)
                return Result;
            else
                return __do_parse_int<view, I + 1, End, Result * 10 + (view[I] - '0')>();
        }

        template<string_literal view, size_t IStart>
        static consteval size_t __do_find_integer_boundary()
        {
            if constexpr (view[IStart] < '0' || view[IStart] > '9' || IStart == view.strlen)
                return IStart;
            else
                return __do_find_integer_boundary<view, IStart + 1>();
        }

        template<string_literal view, size_t I, size_t StartI, Specifier config>
        static consteval auto do_parse_specifier()
        {
            if constexpr (view[I] == '}')
                return tuple_prepend(Formatter<config>{}, do_parse_format_string<view, I + 1, I + 1>());
            else if constexpr (view[I] == '{')
            {
                // dynamic width
                static_assert(view[I + 1] == '}', "Invalid dynamic width specifier");
                return do_parse_specifier<view, I + 2, StartI, SetWidth<config>(Specifier::DYNAMIC)>();
            }
            else if constexpr (view[I] == '.')
            {
                if constexpr (view[I + 1] == '{')
                {
                    // dynamic precision
                    static_assert(view[I + 2] == '}', "Invalid dynamic precision specifier");
                    return do_parse_specifier<view, I + 3, StartI, SetPrecision<config>(Specifier::DYNAMIC)>();
                }
                else if constexpr (view[I + 1] >= '0' && view[I + 1] <= '9')
                {
                    constexpr auto end = __do_find_integer_boundary<view, I + 1>();
                    constexpr size_t value = __do_parse_int<view, I + 1, end>();
                    return do_parse_specifier<view, end, StartI, SetPrecision<config>(value)>();
                }
                else
                {
                    static_assert(false, "Invalid precision specifier");
                }
            }
            else
            {
                // continue current parsing
                if constexpr (view[I] == '<')
                    return do_parse_specifier<view, I + 1, StartI, SetAlignment<config>(Specifier::FormatAlignment::LEFT)>();
                else if constexpr (view[I] == '>')
                    return do_parse_specifier<view, I + 1, StartI, SetAlignment<config>(Specifier::FormatAlignment::RIGHT)>();
                else if constexpr (view[I] == '^')
                    return do_parse_specifier<view, I + 1, StartI, SetAlignment<config>(Specifier::FormatAlignment::CENTER)>();
                else if constexpr (view[I] == '+')
                    return do_parse_specifier<view, I + 1, StartI, SetSign<config>(Specifier::FormatSign::BOTH)>();
                else if constexpr (view[I] == '-')
                    return do_parse_specifier<view, I + 1, StartI, SetSign<config>(Specifier::FormatSign::NEGATIVE)>();
                else if constexpr (view[I] == ' ')
                    return do_parse_specifier<view, I + 1, StartI, SetSign<config>(Specifier::FormatSign::LEADING_SPACE)>();
                else if constexpr (view[I] == '?')
                    return do_parse_specifier<view, I + 1, StartI, SetDebugFormat<config>(true)>();
                else if constexpr (view[I] == '#')
                    return do_parse_specifier<view, I + 1, StartI, SetAlternativeForm<config>(true)>();
                else if constexpr (view[I] == ':')
                    return do_parse_specifier<view, I + 1, StartI, config>();
                else if constexpr (view[I] >= '0' && view[I] <= '9')
                {
                    constexpr auto end = __do_find_integer_boundary<view, I>();
                    constexpr size_t value = __do_parse_int<view, I, end>();
                    return do_parse_specifier<view, end, StartI, SetWidth<config>(value)>();
                }
                else
                    return do_parse_specifier<view, I + 1, StartI, SetFiller<config>(view[I])>();
            }
        }

      private:
        template<size_t StartI, size_t N, typename... Args>
        static inline constexpr auto get_slice(const std::tuple<Args...> &args)
        {
            if constexpr (StartI == N || StartI >= std::tuple_size_v<std::remove_cvref_t<decltype(args)>>)
                return std::tuple();
            else
                return std::tuple_cat(std::make_tuple(std::get<StartI>(args)), get_slice<StartI + 1, N>(args));
        }

        template<typename... TArgs>
        static inline constexpr auto first_arg(const std::tuple<TArgs...> &args)
        {
            return std::get<0>(args);
        }

        template<size_t N, typename... TArgs>
        static inline constexpr auto first_n_args(const std::tuple<TArgs...> &args)
        {
            return get_slice<0, N>(args);
        }

        template<typename... TArgs>
        static inline constexpr auto rest_args(const std::tuple<TArgs...> &args)
        {
            return get_slice<1, std::tuple_size_v<std::remove_cvref_t<decltype(args)>>>(args);
        }

        template<size_t N, typename... TArgs>
        static inline constexpr auto rest_args_from(const std::tuple<TArgs...> &args)
        {
            return get_slice<N, std::tuple_size_v<std::remove_cvref_t<decltype(args)>>>(args);
        }

        template<typename Stream, typename... TArgs>
        static inline constexpr auto do_next_print(Stream &stream, std::tuple<TArgs...> str_format_tuple)
        {
            constexpr auto rest_specifier_size = std::tuple_size_v<std::remove_cvref_t<decltype(str_format_tuple)>>;
            static_assert(rest_specifier_size == 0 || rest_specifier_size == 1, "Invalid format string");

            if constexpr (rest_specifier_size == 1)
                stream << first_arg(str_format_tuple);
        }

        template<typename Stream, typename... TSpecs, typename TSpec, typename TArg, typename... TArgs>
        static inline constexpr auto do_next_print(Stream &stream, TSpec spec, const std::tuple<TSpecs...> &specs, const TArg &arg, const std::tuple<TArgs...> &args)
        {
            constexpr auto rest_specifier_size = std::tuple_size_v<std::remove_cvref_t<decltype(specs)>>;
            constexpr auto rest_arg_size = std::tuple_size_v<std::remove_cvref_t<decltype(args)>>;

            if constexpr (std::is_same_v<TSpec, mos::string_view>)
            {
                stream << spec;
                do_next_print(stream, first_arg(specs), rest_args(specs), arg, args); // print string, keeps args
            }
            else if constexpr (is_formatter_v<TSpec>())
            {
                using TArgTuple = decltype(TSpec::template arg_tuple<TArg>());
                constexpr auto TArgTupleSize = std::tuple_size_v<TArgTuple>;
                static_assert(TArgTupleSize == 1 || TArgTupleSize == 2 || TArgTupleSize == 3, "Invalid number of arguments");

                static_assert(TArgTupleSize - 1 <= rest_arg_size, "Not enough arguments");

                spec.template Print<TArg>(stream, std::tuple_cat(std::make_tuple(arg), first_n_args<TArgTupleSize - 1>(args)));

                if constexpr (rest_arg_size - (TArgTupleSize - 1) > 0)
                {
                    // print formatter, keeps args
                    do_next_print(stream, first_arg(specs), rest_args(specs), std::get<TArgTupleSize - 1>(args), rest_args_from<TArgTupleSize>(args));
                }
                else
                {
                    static_assert(rest_specifier_size == 0 || rest_specifier_size == 1, "Invalid number of arguments");
                    do_next_print(stream, specs);
                }
            }
            else
            {
                static_assert(false, "Invalid specifier");
            }
        }

      public:
        template<string_literal view, size_t I = 0, size_t StartI = 0>
        static inline consteval auto do_parse_format_string()
        {
            if constexpr (I == view.strlen)
            {
                if constexpr (StartI == I)
                    return std::make_tuple();
                else
                    return std::make_tuple(mos::string_view{ view.at(StartI), view.at(I) });
            }
            else if constexpr (view[I] == '{' && (I < 1 || view[I - 1] != '\\'))
            {
                // remove forward slash if being escaped
                constexpr mos::string_view string{ view.at(StartI), view.at(I) };
                if constexpr (StartI == I)
                    return do_parse_specifier<view, I + 1, I, Specifier{}>();
                else
                    return tuple_prepend(string, do_parse_specifier<view, I + 1, I, Specifier{}>());
            }
            else
            {
                constexpr bool escaped = view[I] == '{' && (I >= 1 && view[I - 1] == '\\');
                return do_parse_format_string<view, I + 1, StartI + escaped>();
            }
        }

        template<mos::string_literal view, typename Stream, typename... TArgs>
        static inline constexpr Stream &print_literal(Stream &stream, TArgs... args)
        {
            constexpr auto format = do_parse_format_string<view>();
            constexpr auto args_expected = n_args_expected(format);
            static_assert(args_expected == sizeof...(args), "Number of arguments does not match format string");

            if constexpr (args_expected == 0)
            {
                do_next_print(stream, format);
            }
            else
            {
                const auto args_tuple = std::make_tuple(args...);
                do_next_print(stream, first_arg(format), rest_args(format), first_arg(args_tuple), rest_args(args_tuple));
            }
            return stream;
        }

        template<typename M, typename Stream, typename... TArgs>
        static inline constexpr Stream &print_m(Stream &stream, std::tuple<TArgs...> args)
        {
            constexpr auto format = do_parse_format_string<M::string_value()>();
            constexpr auto args_expected = n_args_expected(format);
            static_assert(args_expected == sizeof...(TArgs), "Number of arguments does not match format string");

            if constexpr (args_expected == 0)
            {
                do_next_print(stream, format);
            }
            else
            {
                do_next_print(stream, first_arg(format), rest_args(format), first_arg(args), rest_args(args));
            }

            return stream;
        }

        struct formatted_string
        {
        };
    };

} // namespace mos

#define formatted_type(literal)                                                                                                                                          \
    []()                                                                                                                                                                 \
    {                                                                                                                                                                    \
        struct M : mos::FormatImpl::formatted_string                                                                                                                     \
        {                                                                                                                                                                \
            static consteval auto string_value()                                                                                                                         \
            {                                                                                                                                                            \
                return ::mos::string_literal{ literal };                                                                                                                 \
            }                                                                                                                                                            \
        };                                                                                                                                                               \
        return M{};                                                                                                                                                      \
    }()

#if 0
struct FakeStreamType
{
    FakeStreamType &operator<<(int);
    FakeStreamType &operator<<(mos::string_view);
};

inline auto FakeStream = FakeStreamType();

void testfunc()
{
    mos::FormatImpl::print_literal<"\\{ 'TODO: IO.Name', {}}">(FakeStream, 1 == 0 ? "A" : "B");
}
#endif
