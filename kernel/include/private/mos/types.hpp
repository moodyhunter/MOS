// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <cstddef>
#include <mos/type_utils.hpp>
#include <mos/types.h>
#include <stdnoreturn.h>
#include <type_traits>

// for C++, we need to use the atomic type directly
typedef std::atomic_size_t atomic_t;

template<class P, class M>
[[gnu::always_inline]] constexpr size_t __offsetof(const M P::*member)
{
    return (size_t) &(reinterpret_cast<P *>(0)->*member);
}

template<class P, class M>
[[gnu::always_inline]] constexpr inline P *__container_of(M *ptr, const M P::*member)
{
    return (P *) ((char *) ptr - __offsetof(member));
}

template<class P, class M>
[[gnu::always_inline]] constexpr inline const P *__container_of(const M *ptr, const M P::*member)
{
    return (const P *) ((char *) ptr - __offsetof(member));
}

#define container_of(ptr, type, member) __container_of(ptr, &type::member)

template<typename TOut, typename TIn>
[[gnu::always_inline]] inline TOut *cast(TIn *value)
{
    return reinterpret_cast<TOut *>(value);
}

template<typename TOut, typename TIn>
[[gnu::always_inline]] inline const TOut *cast(const TIn *value)
{
    return reinterpret_cast<const TOut *>(value);
}

struct PtrResultBase
{
  protected:
    const int errorCode;

    PtrResultBase() : errorCode(0) {};
    PtrResultBase(int errorCode) : errorCode(errorCode) {};

  public:
    virtual bool isErr() const final
    {
        return errorCode != 0;
    }

    virtual long getErr() const final
    {
        return errorCode;
    }

    explicit operator bool() const
    {
        return !isErr();
    }
};

template<typename T>
struct PtrResult : public PtrResultBase
{
  private:
    T *const value;

  public:
    PtrResult(T *value) : PtrResultBase(0), value(value) {};
    PtrResult(int errorCode) : PtrResultBase(errorCode), value(nullptr) {};

    template<typename U>
    requires std::is_base_of_v<T, U> PtrResult(PtrResult<U> other) : PtrResultBase(other.getErr()), value(other.get()) {};

  public:
    std::add_lvalue_reference<T>::type operator*()
    {
        return *value;
    }

    const std::add_lvalue_reference<T>::type &operator*() const
    {
        return *value;
    }

    T *operator->()
    {
        return value;
    }

    const T *operator->() const
    {
        return value;
    }

    T *get() const
    {
        if (isErr())
            mos::__raise_bad_ptrresult_value(errorCode);
        return value;
    }

    bool operator==(const std::nullptr_t) const
    {
        return value == nullptr;
    }

    bool operator==(const PtrResult<T> &other) const
    {
        return value == other.value && errorCode == other.errorCode;
    }

    bool operator==(const T *other) const
    {
        return value == other;
    }

    explicit operator bool() const
    {
        return value != nullptr && !isErr();
    }
};

template<>
struct PtrResult<void> : public PtrResultBase
{
  public:
    PtrResult() : PtrResultBase(0) {};
    PtrResult(int errorCode) : PtrResultBase(errorCode) {};
};

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
