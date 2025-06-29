// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types/ResultBase.hpp"

#include <mos/cpp_support.hpp>
#include <type_traits>

template<typename T>
requires(!std::is_void_v<T> && !std::is_pointer_v<T> && !std::is_reference_v<T> && !std::is_array_v<T>) struct ValueResult : public ResultBase
{
  private:
    T value;

  public:
    struct is_result_tag
    {
        explicit is_result_tag() = default;
    };
    static constexpr is_result_tag is_result{};

  public:
    // when T is not arithmetic, we can enable the one-argument constructor
    template<typename U = T>
    ValueResult(typename std::enable_if_t<!std::is_arithmetic_v<U>, U>) : ResultBase(0), value(value){};

    template<typename U = T>
    ValueResult(typename std::enable_if_t<!std::is_arithmetic_v<U>, long>) : ResultBase(errorCode), value(){};

    // tagged constructor to allow explicit construction from a value
    ValueResult(T value, is_result_tag) : ResultBase(0), value(value) {};

    // constructor for error case
    template<typename U>
    requires std::is_same_v<U, ResultBase> ValueResult(U &&other) : ResultBase(other.getErr()), value()
    {
        if (!isErr())
            mos::__raise_bad_result_value(errorCode); // may only be called if incoming ResultBase is an error
    }

  public:
    T &get()
    {
        if (isErr())
            mos::__raise_bad_result_value(errorCode);
        return value;
    }

    const T &get() const
    {
        if (isErr())
            mos::__raise_bad_result_value(errorCode);
        return value;
    }

    bool operator==(const std::nullptr_t) const
    {
        return false; // Result cannot be compared to nullptr
    }

    bool operator==(const ValueResult<T> &other) const
    {
        return value == other.value && errorCode == other.errorCode;
    }

    explicit operator bool() const
    {
        return !isErr();
    }

    auto match(auto &&onOk, auto &&onErr) const
    {
        if (isErr())
            return onErr(errorCode);
        else
            return onOk(value);
    }
};
