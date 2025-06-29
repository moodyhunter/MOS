// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <mos/types.h>
#include <type_traits>

template<typename E>
requires std::is_enum_v<E> struct Flags
{
  private:
    E value_;
    static_assert(sizeof(E) <= sizeof(u32), "Flags only supports enums that fit into a u32");

    using EnumType = E;
    using U = std::underlying_type_t<E>;

  public:
    constexpr Flags(E value = static_cast<E>(0)) : value_(value)
    {
        static_assert(sizeof(Flags) == sizeof(E), "Flags must have the same size as the enum");
    }

    ~Flags() = default;

    static Flags all()
    {
        return static_cast<E>(~static_cast<U>(0));
    }

    inline operator U() const
    {
        return static_cast<U>(value_);
    }

    inline Flags operator|(E e) const
    {
        return static_cast<E>(static_cast<U>(value_) | static_cast<U>(e));
    }

    inline Flags operator&(E b) const
    {
        return static_cast<E>(static_cast<U>(value_) & static_cast<U>(b));
    }

    inline Flags operator&(Flags b) const
    {
        return static_cast<E>(static_cast<U>(value_) & static_cast<U>(b.value_));
    }

    inline bool test(E b) const
    {
        return static_cast<U>(value_) & static_cast<U>(b);
    }

    inline bool test_inverse(E b) const
    {
        return static_cast<U>(value_) & ~static_cast<U>(b);
    }

    inline Flags erased(E b) const
    {
        return static_cast<E>(static_cast<U>(value_) & ~static_cast<U>(b));
    }

    inline Flags erased(Flags b) const
    {
        return static_cast<E>(static_cast<U>(value_) & ~static_cast<U>(b.value_));
    }

    inline Flags &operator|=(E b)
    {
        value_ = static_cast<E>(static_cast<U>(value_) | static_cast<U>(b));
        return *this;
    }

    inline Flags &operator&=(E b)
    {
        value_ = static_cast<E>(static_cast<U>(value_) & static_cast<U>(b));
        return *this;
    }

    inline Flags &operator&=(Flags b)
    {
        value_ = static_cast<E>(static_cast<U>(value_) & static_cast<U>(b.value_));
        return *this;
    }

    inline Flags erase(E b)
    {
        value_ = static_cast<E>(static_cast<U>(value_) & ~static_cast<U>(b));
        return *this;
    }

    inline Flags erase(Flags b)
    {
        value_ = static_cast<E>(static_cast<U>(value_) & ~static_cast<U>(b.value_));
        return *this;
    }
};

namespace std
{
    template<typename E>
    struct hash<Flags<E>>
    {
        size_t operator()(const Flags<E> &flags) const noexcept
        {
            return static_cast<size_t>(flags);
        }
    };
} // namespace std

#define MOS_ENUM_FLAGS(enum, flags) using flags = Flags<enum>

template<typename E>
requires std::is_enum_v<E> constexpr Flags<E> operator|(E a, E b)
{
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(a) | static_cast<U>(b));
}

template<typename E>
requires std::is_enum_v<E> constexpr void operator~(E a) = delete;

template<typename E>
requires std::is_enum_v<E> constexpr void operator&(E a, E b) = delete;
