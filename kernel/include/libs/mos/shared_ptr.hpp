// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <mos/allocator.hpp>
#include <mos/type_utils.hpp>
#include <mos/types.hpp>
#include <type_traits>

namespace mos
{
    // Forward declaration for shared_ptr
    template<typename T>
    class shared_ptr;

    // Forward declaration for weak_ptr
    template<typename T>
    class weak_ptr;

    template<typename T, typename... Args>
    T *create(Args &&...args);

    template<typename T>
    struct __shared_ptr_core : mos::NamedType<"shared_ptr.core">
    {
        explicit __shared_ptr_core(T *ptr, size_t own, size_t weak) : _ptr(ptr), _n_own(own), _n_weak(weak) {};
        ~__shared_ptr_core()
        {
            delete _ptr;
        }

        // clang-format off
        void _dec_weak() { _n_weak--; }
        void _inc_weak() { _n_weak++; }
        void _inc_use() { _n_own++; }
        void _dec_use() { _n_own--; }
        // clang-format on

        bool _can_delete() const
        {
            return _n_own == 0;
        }

        size_t _use_count() const
        {
            return _n_own;
        }

        size_t _weak_count() const
        {
            return _n_weak;
        }

        friend bool operator==(const __shared_ptr_core &lhs, const __shared_ptr_core &rhs)
        {
            return lhs._ptr == rhs._ptr;
        }

      public:
        T *_ptr;

      private:
        size_t _n_own;  // number of shared_ptr instances owning the object
        size_t _n_weak; // number of weak_ptr instances owning the object
    };

    template<typename T>
    class weak_ptr
    {
        friend class shared_ptr<T>;

      public:
        using element_type = std::remove_extent_t<T>;

      public:
        constexpr weak_ptr() : _c(nullptr) {};
        constexpr weak_ptr(std::nullptr_t) : _c(nullptr) {};

        weak_ptr(const weak_ptr &r) noexcept : _c(r._c)
        {
            _c->_inc_weak();
        }

        template<class Y>
        weak_ptr(const weak_ptr<Y> &r) noexcept : _c(r._c)
        {
            _c->_inc_weak();
        }

        template<class Y>
        weak_ptr(const shared_ptr<Y> &r) noexcept : _c(r._c)
        {
            _c->_inc_weak();
        }

        weak_ptr(weak_ptr &&r) noexcept : _c(r._c)
        {
            r._c = nullptr;
        }

        template<class Y>
        weak_ptr(weak_ptr<Y> &&r) noexcept : _c(r._c)
        {
            r._c = nullptr;
        }

      public:
        long use_count() const
        {
            if (_c == nullptr)
                return 0;
            return _c->_n_own;
        }

        bool expired() const
        {
            return use_count() == 0;
        }

        shared_ptr<T> lock()
        {
            if (_c->_use_count() == 0)
                return nullptr;
            return shared_ptr<T>(_c);
        }

        ~weak_ptr()
        {
            _c->_dec_weak();
            if (_c->_can_delete())
                delete _c;
        }

      private:
        __shared_ptr_core<T> *_c;
    };

    template<typename T>
    class shared_ptr
    {
        friend class weak_ptr<T>;

      public:
        using element_type = std::remove_extent_t<T>;

      public:
        constexpr shared_ptr() noexcept : _c(nullptr) {};
        constexpr shared_ptr(std::nullptr_t) noexcept : _c(nullptr) {};

        template<typename Y>
        explicit shared_ptr(Y *ptr) : _c(mos::create<__shared_ptr_core<T>>(ptr, 1, 0)){};

        explicit shared_ptr(__shared_ptr_core<T> *cb) : _c(cb)
        {
            if (_c)
                _c->_inc_use();
        }

        shared_ptr(const shared_ptr &r) noexcept : _c(r._c)
        {
            if (_c)
                _c->_inc_use();
        }

        template<typename Y>
        shared_ptr(shared_ptr<Y> &other) : _c(other._c)
        {
            if (_c)
                _c->_inc_use();
        }

        shared_ptr(shared_ptr &&r) noexcept : _c(r._c)
        {
            r._c = nullptr;
        }

        template<typename Y>
        shared_ptr(shared_ptr<Y> &&other) : _c(other._c)
        {
            other._c = nullptr;
        }

      public:
        shared_ptr &operator=(const shared_ptr &r) noexcept
        {
            if (_c == r._c)
                return *this;

            if (_c)
            {
                _c->_dec_use();
                if (_c->_can_delete())
                    delete _c;
            }

            _c = r._c;
            if (_c)
                _c->_inc_use();

            return *this;
        }

        template<class Y>
        shared_ptr &operator=(const shared_ptr<Y> &r) noexcept
        {
            if (_c == r._c)
                return *this;

            if (_c)
            {
                _c->_dec_use();
                if (_c->_can_delete())
                    delete _c;
            }

            _c = r._c;
            if (_c)
                _c->_inc_use();

            return *this;
        }

        shared_ptr &operator=(shared_ptr &&r) noexcept
        {
            if (_c == r._c)
                return *this;

            if (_c)
            {
                _c->_dec_use();
                if (_c->_can_delete())
                    delete _c;
            }

            _c = r._c;
            r._c = nullptr;

            return *this;
        }

        template<class Y>
        shared_ptr &operator=(shared_ptr<Y> &&r) noexcept
        {
            if (_c == r._c)
                return *this;

            if (_c)
            {
                _c->_dec_use();
                if (_c->_can_delete())
                    delete _c;
            }

            _c = r._c;
            r._c = nullptr;

            return *this;
        }

      public:
        ~shared_ptr()
        {
            if (_c == nullptr)
                return; // nothing to do, we represents nullptr

            _c->_dec_use();
            if (_c->_can_delete())
                delete _c;
        }

      public:
        long use_count() const
        {
            if (_c == nullptr)
                return 0;
            return _c->_use_count();
        }

        void reset()
        {
            if (_c == nullptr)
                return;
            _c->_dec_use();
            if (_c->_can_delete())
                delete _c;
            _c = nullptr;
        }

        T *release()
        {
            if (_c == nullptr)
                return nullptr;
            T *ptr = _c->_ptr;
            _c->_dec_use();
            if (_c->_can_delete())
                delete _c;
            _c = nullptr;
            return ptr;
        }

      public:
        element_type *get() const
        {
            if (_c == nullptr)
                return nullptr;
            return _c->_ptr;
        }

        element_type &operator*() const
        {
            return *_c->_ptr;
        }

        element_type *operator->() const
        {
            if (_c == nullptr)
                return nullptr;
            return _c->_ptr;
        }

      public:
        friend void swap(shared_ptr &lhs, shared_ptr &rhs)
        {
            std::swap(lhs._c, rhs._c);
        }

        friend bool operator==(const shared_ptr &lhs, const shared_ptr &rhs)
        {
            return lhs._c == rhs._c && *lhs._c == *rhs._c;
        }

        friend bool operator==(const shared_ptr &lhs, std::nullptr_t)
        {
            return lhs._c == nullptr;
        }

        operator bool() const
        {
            return _c != nullptr;
        }

      private:
        __shared_ptr_core<T> *_c;
    };

    template<typename T, typename... Args>
    requires HasTypeName<T> shared_ptr<T> make_shared(Args &&...args)
    {
        return shared_ptr<T>(mos::create<T>(std::forward<Args>(args)...));
    }
} // namespace mos

template<typename T>
using ptr = mos::shared_ptr<T>;

template<typename T>
struct PtrResult<mos::shared_ptr<T>>
{
  private:
    const mos::shared_ptr<T> value;
    const int errorCode;

  public:
    PtrResult() : value(nullptr) {};
    PtrResult(const mos::shared_ptr<T> &value) : value(value), errorCode(0) {};
    PtrResult(const mos::shared_ptr<T> &&value) : value(std::move(value)), errorCode(0) {};
    PtrResult(int errorCode) : value(nullptr), errorCode(errorCode) {};

  public:
    mos::shared_ptr<T> get() const
    {
        return value;
    }

    bool isErr() const
    {
        return errorCode != 0;
    }

    long getErr() const
    {
        return errorCode;
    }
};
