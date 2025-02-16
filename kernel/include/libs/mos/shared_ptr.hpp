// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <mos/allocator.hpp>
#include <mos/type_utils.hpp>
#include <type_traits>

namespace mos
{
    // Forward declaration for shared_ptr
    template<typename T>
    class shared_ptr;

    // Forward declaration for weak_ptr
    template<typename T>
    class weak_ptr;

    template<typename T>
    struct __shared_ptr_core : mos::NamedType<"__shared_ptr_core">
    {
        explicit __shared_ptr_core(T *ptr, size_t own, size_t weak) : ptr(ptr), _n_own(own), _n_weak(weak) {};
        ~__shared_ptr_core()
        {
            delete ptr;
        }

        // clang-format off
        void _dec_weak() { _n_weak--; }
        void _inc_weak() { _n_weak++; }
        void _inc_use() { _n_own++; }
        void _dec_use() { _n_own--; }
        // clang-format on

        bool _can_delete() const
        {
            return _n_own == 0 && _n_weak == 0;
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
            return lhs.ptr == rhs.ptr;
        }

      public:
        T *ptr;

      private:
        size_t _n_own;  // number of shared_ptr instances owning the object
        size_t _n_weak; // number of weak_ptr instances owning the object
    };

    template<typename T>
    class weak_ptr
    {
      public:
        using element_type = std::remove_extent_t<T>;

      public:
        constexpr weak_ptr() : _cb(nullptr) {};
        constexpr weak_ptr(std::nullptr_t) : _cb(nullptr) {};

        weak_ptr(const weak_ptr &r) noexcept : _cb(r._cb)
        {
            _cb->_inc_weak();
        }

        template<class Y>
        weak_ptr(const weak_ptr<Y> &r) noexcept : _cb(r._cb)
        {
            _cb->_inc_weak();
        }

        template<class Y>
        weak_ptr(const mos::shared_ptr<Y> &r) noexcept : _cb(r._c)
        {
            _cb->_inc_weak();
        }

        weak_ptr(weak_ptr &&r) noexcept : _cb(r._cb)
        {
            r._cb = nullptr;
        }

        template<class Y>
        weak_ptr(weak_ptr<Y> &&r) noexcept : _cb(r._cb)
        {
            r._cb = nullptr;
        }

      public:
        long use_count() const
        {
            if (_cb == nullptr)
                return 0;
            return _cb->_n_own;
        }

        bool expired() const
        {
            return use_count() == 0;
        }

        shared_ptr<T> lock()
        {
            if (_cb->_n_own == 0)
                return nullptr;
            return shared_ptr<T>(_cb->ptr, _cb);
        }

        ~weak_ptr()
        {
            _cb->_n_weak--;
            if (_cb->_can_delete())
                delete _cb;
        }

      private:
        __shared_ptr_core<T> *_cb;
    };

    template<typename T>
    class shared_ptr
    {
      public:
        using element_type = std::remove_extent_t<T>;

      public:
        constexpr shared_ptr() noexcept : _c(nullptr) {};
        constexpr shared_ptr(std::nullptr_t) noexcept : _c(nullptr) {};

        template<typename Y>
        explicit shared_ptr(Y *ptr) : _c(mos::create<__shared_ptr_core<T>>(ptr, 1, 0)){};

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

      public:
        element_type *get() const
        {
            if (_c == nullptr)
                return nullptr;
            return _c->ptr;
        }

        element_type &operator*() const
        {
            return *_c->ptr;
        }

        element_type *operator->() const
        {
            if (_c == nullptr)
                return nullptr;
            return _c->ptr;
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

inline void X()
{
    mos::shared_ptr<int> ptr;
}
