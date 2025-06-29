// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types/ResultBase.hpp"

#include <mos/cpp_support.hpp>
#include <type_traits>

template<typename T>
struct PtrResult : public ResultBase
{
  private:
    T *const value;

  public:
    PtrResult(T *value) : ResultBase(0), value(value) {};
    PtrResult(long errorCode) : ResultBase(errorCode), value(nullptr) {};

    template<typename U>
    requires std::is_base_of_v<T, U> PtrResult(PtrResult<U> other) : ResultBase(other.getErr()), value(other.get()) {};

    // constructor for error case
    template<typename U>
    requires std::is_same_v<U, ResultBase> PtrResult(U &&other) : ResultBase(other.getErr()), value()
    {
        if (!isErr())
            mos::__raise_bad_result_value(errorCode); // may only be called if incoming ResultBase is an error
    }

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
            mos::__raise_bad_result_value(errorCode);
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

    auto match(auto &&onOk, auto &&onErr) const
    {
        if (isErr())
            return onErr(errorCode);
        else
            return onOk(value);
    }
};

template<>
struct PtrResult<void> : public ResultBase
{
  public:
    PtrResult() : ResultBase(0) {};
    PtrResult(int errorCode) : ResultBase(errorCode) {};

  private:
    template<typename U>
    requires std::is_same_v<U, ResultBase> PtrResult(U &&other) : ResultBase(other.getErr())
    {
        if (!isErr())
            mos::__raise_bad_result_value(errorCode); // may only be called if incoming ResultBase is an error
    }
};
