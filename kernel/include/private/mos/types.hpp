// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <cstddef>
#include <mos/types.h>
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

// enum operators are not supported in C++ implicitly
#define MOS_ENUM_OPERATORS(_enum)                                                                                                                                        \
    constexpr inline _enum operator|(_enum a, _enum b)                                                                                                                   \
    {                                                                                                                                                                    \
        return static_cast<_enum>(static_cast<std::underlying_type_t<_enum>>(a) | static_cast<std::underlying_type_t<_enum>>(b));                                        \
    }                                                                                                                                                                    \
    constexpr inline _enum operator&(_enum a, _enum b)                                                                                                                   \
    {                                                                                                                                                                    \
        return static_cast<_enum>(static_cast<std::underlying_type_t<_enum>>(a) & static_cast<std::underlying_type_t<_enum>>(b));                                        \
    }                                                                                                                                                                    \
    constexpr inline _enum operator~(_enum a)                                                                                                                            \
    {                                                                                                                                                                    \
        return static_cast<_enum>(~static_cast<std::underlying_type_t<_enum>>(a));                                                                                       \
    }                                                                                                                                                                    \
    constexpr inline _enum &operator|=(_enum &a, _enum b)                                                                                                                \
    {                                                                                                                                                                    \
        return a = a | b;                                                                                                                                                \
    }                                                                                                                                                                    \
    constexpr inline _enum &operator&=(_enum &a, _enum b)                                                                                                                \
    {                                                                                                                                                                    \
        return a = a & b;                                                                                                                                                \
    }                                                                                                                                                                    \
    constexpr inline _enum &operator|=(_enum &a, std::underlying_type_t<_enum> b)                                                                                        \
    {                                                                                                                                                                    \
        return a = a | static_cast<_enum>(b);                                                                                                                            \
    }                                                                                                                                                                    \
    constexpr inline _enum &operator&=(_enum &a, std::underlying_type_t<_enum> b)                                                                                        \
    {                                                                                                                                                                    \
        return a = a & static_cast<_enum>(b);                                                                                                                            \
    }
