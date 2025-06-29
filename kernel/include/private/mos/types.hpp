// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <mos/type_utils.hpp>
#include <mos/types.h>
#include <mos/types/PtrResult.hpp>
#include <mos/types/ResultBase.hpp>
#include <mos/types/ValueResult.hpp>
#include <mos/types/container_of.hpp>
#include <mos/types/flags.hpp>
#include <stdnoreturn.h>
#include <type_traits>

// for C++, we need to use the atomic type directly
typedef std::atomic_size_t atomic_t;

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

template<typename T>
requires(!std::is_void_v<T>) extern inline auto Ok(T value)
{
    if constexpr (std::is_pointer_v<T>)
        return PtrResult<std::remove_pointer_t<T>>(value);
    else
        return ValueResult<T>(value, ValueResult<T>::is_result);
}
