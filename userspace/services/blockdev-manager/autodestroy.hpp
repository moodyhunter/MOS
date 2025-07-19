// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <iostream>

class ScopeGuard
{
  public:
    ScopeGuard(std::function<void()> func) : _func(func){};
    ~ScopeGuard()
    {
        _func();
    };

  private:
    ScopeGuard(const ScopeGuard &) = delete;
    ScopeGuard &operator=(const ScopeGuard &) = delete;
    ScopeGuard(ScopeGuard &&) = delete;
    ScopeGuard &operator=(ScopeGuard &&) = delete;

  private:
    std::function<void()> _func;
} __attribute__((__warn_unused__));

constexpr auto mScopeGuard = [](auto &&l) { return ScopeGuard(std::forward<decltype(l)>(l)); };
constexpr auto mAutoDestroy = [](auto &ptr, auto &&d) { return ScopeGuard([&ptr, lambda = std::forward<decltype(d)>(d)]() { lambda(ptr), ptr = nullptr; }); };
