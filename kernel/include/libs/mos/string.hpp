// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <mos/allocator.hpp>
#include <mos/string_view.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

namespace mos
{
    constexpr size_t short_string_capacity = 16;

    template<typename Char, typename TAllocator>
    class mos_basic_string
    {
        bool _is_long = false;
        TAllocator _allocator;

        size_t _length; ///< string length excluding null terminator

        union
        {
            struct _long_buffer
            {
                Char *_buffer;
                size_t _capacity; ///< buffer capacity includes null terminator
            } _long;

            struct _short_buffer
            {
                Char _buffer[short_string_capacity];
            } _short;
        } _data;

      public:
        mos_basic_string(std::nullptr_t) = delete;

        mos_basic_string(TAllocator allocator = TAllocator()) : _allocator(std::move(allocator)), _length{ 0 }, _is_long(false) {};

        mos_basic_string(const Char *buffer, TAllocator allocator = TAllocator()) : _allocator(std::move(allocator))
        {
            _init(buffer, generic_strlen(buffer));
        }

        explicit mos_basic_string(const Char *buffer, size_t size, TAllocator allocator = TAllocator()) : _allocator(std::move(allocator))
        {
            _init(buffer, size);
        }

        explicit mos_basic_string(const basic_string_view<Char> &view, TAllocator allocator = TAllocator()) : _allocator(std::move(allocator))
        {
            _init(view.data(), view.size());
        }

        mos_basic_string(size_t size, Char c = 0, TAllocator allocator = TAllocator()) : _allocator(std::move(allocator))
        {
            _init(&c, size);
        }

        mos_basic_string(const mos_basic_string &other) : _allocator(other._allocator)
        {
            _init(other.data(), other.size());
        }

        mos_basic_string(mos_basic_string &&other) : _allocator(std::move(other._allocator))
        {
            _length = other._length;
            _is_long = other._is_long;
            if (_is_long)
            {
                _data._long._capacity = other._data._long._capacity;
                _data._long._buffer = other._data._long._buffer;
                other._data._long._buffer = nullptr; // detach
            }
            else
            {
                for (size_t i = 0; i < short_string_capacity; i++)
                    _data._short._buffer[i] = other._data._short._buffer[i];
            }
        }

        ~mos_basic_string()
        {
            if (_is_long && _data._long._buffer)
                _allocator.free(_data._long._buffer);
        }

        mos_basic_string &operator=(const Char *buffer)
        {
            this->~mos_basic_string();
            this->_allocator = TAllocator();
            this->_length = generic_strlen(buffer);
            this->_is_long = this->_length >= short_string_capacity;
            if (this->_is_long)
            {
                this->_data._long._capacity = this->_length + 1;
                this->_data._long._buffer = (Char *) _allocator.allocate(sizeof(Char) * this->_data._long._capacity);
                memcpy(this->_data._long._buffer, buffer, sizeof(Char) * this->_length);
                this->_data._long._buffer[this->_length] = 0;
            }
            else
            {
                for (size_t i = 0; i < short_string_capacity; i++)
                    this->_data._short._buffer[i] = buffer[i];
            }
            return *this;
        }

        mos_basic_string &operator=(const mos::basic_string_view<Char> &view)
        {
            this->~mos_basic_string();
            this->_allocator = TAllocator();
            this->_length = view.size();
            this->_is_long = this->_length >= short_string_capacity;
            if (this->_is_long)
            {
                this->_data._long._capacity = this->_length + 1;
                this->_data._long._buffer = (Char *) _allocator.allocate(sizeof(Char) * this->_data._long._capacity);
                memcpy(this->_data._long._buffer, view.data(), sizeof(Char) * this->_length);
                this->_data._long._buffer[this->_length] = 0;
            }
            else
            {
                for (size_t i = 0; i < short_string_capacity; i++)
                    this->_data._short._buffer[i] = view.data()[i];
            }
            return *this;
        }

        mos_basic_string &operator=(mos_basic_string &&other)
        {
            this->~mos_basic_string();
            this->_allocator = std::move(other._allocator);
            this->_length = other._length;
            this->_is_long = other._is_long;
            if (this->_is_long)
            {
                this->_data._long._capacity = other._data._long._capacity;
                this->_data._long._buffer = other._data._long._buffer;
                other._data._long._buffer = nullptr; // detach
            }
            else
            {
                for (size_t i = 0; i < short_string_capacity; i++)
                    this->_data._short._buffer[i] = other._data._short._buffer[i];
            }
            return *this;
        }

        mos_basic_string &operator=(const mos_basic_string &other)
        {
            _init(other.data(), other.size());
            return *this;
        }

        friend mos_basic_string operator+(const basic_string_view<Char> &lhs, const mos_basic_string &rhs)
        {
            return mos_basic_string(lhs) + rhs;
        }

        void resize(size_t new_length)
        {
            if (new_length == _length)
                return;
            if (new_length < short_string_capacity)
                _convert_to_short();
            else
                _convert_to_long();
            _length = new_length;
        }

        mos_basic_string operator+(const mos_basic_string &other) const
        {
            auto copy = *this;
            return copy += other;
        }

        mos_basic_string operator+(Char c) const
        {
            return operator+(basic_string_view(&c, 1));
        }

        void push_back(Char c)
        {
            operator+=(c);
        }

        mos_basic_string &operator+=(const basic_string_view<Char> &other)
        {
            const auto new_length = _length + other.size();
            if (new_length < short_string_capacity)
            {
                memcpy(_data._short._buffer + _length, other.data(), sizeof(Char) * other.size());
                _data._short._buffer[new_length] = 0;
                _length = new_length;
            }
            else
            {
                _convert_to_long(new_length);
                memcpy(_data._long._buffer + _length, other.data(), sizeof(Char) * other.size());
                _data._long._buffer[new_length] = 0;
                _length = new_length;
            }

            return *this;
        }

        mos_basic_string &operator+=(Char c)
        {
            return operator+=(basic_string_view(&c, 1));
        }

        const Char *data() const
        {
            return _is_long ? _data._long._buffer : _data._short._buffer;
        }

        const Char *c_str() const
        {
            return data();
        }

        Char &operator[](size_t index)
        {
            return _is_long ? _data._long._buffer[index] : _data._short._buffer[index];
        }

        const Char &operator[](size_t index) const
        {
            return _is_long ? _data._long._buffer[index] : _data._short._buffer[index];
        }

        size_t size() const
        {
            return _length;
        }

        bool empty() const
        {
            return _length == 0;
        }

        Char *begin()
        {
            return _is_long ? &_data._long._buffer[0] : &_data._short._buffer[0];
        }

        const Char *begin() const
        {
            return _is_long ? &_data._long._buffer[0] : &_data._short._buffer[0];
        }

        Char *end()
        {
            return _is_long ? &_data._long._buffer[_length] : &_data._short._buffer[_length];
        }

        const Char *end() const
        {
            return _is_long ? &_data._long._buffer[_length] : &_data._short._buffer[_length];
        }

        bool operator==(const mos_basic_string &other) const
        {
            return _do_compare(other.data(), other.size()) == 0;
        }

        bool operator==(const char *rhs) const
        {
            return _do_compare(rhs, generic_strlen(rhs)) == 0;
        }

        operator basic_string_view<Char>() const
        {
            return basic_string_view<Char>(data(), _length);
        }

        bool operator==(std::nullptr_t) const = delete;

      private:
        void _init(const Char *buffer, size_t size)
        {
            _length = size;
            if (_length < short_string_capacity)
            {
                for (size_t i = 0; i < _length; i++)
                    _data._short._buffer[i] = buffer[i];
                _data._short._buffer[_length] = 0;
                _is_long = false;
            }
            else
            {
                _data._long._capacity = sizeof(Char) * _length + 1;
                _data._long._buffer = (Char *) _allocator.allocate(_data._long._capacity);
                memcpy(_data._long._buffer, buffer, sizeof(Char) * size);
                _data._long._buffer[size] = 0;
                _is_long = true;
            }
        }
        int _do_compare(const Char *other, const size_t other_size) const
        {
            if (_length != other_size)
                return _length < other_size ? -1 : 1;

            return generic_strncmp(data(), other, _length);
        }

        void _convert_to_long(size_t new_length = 0)
        {
            if (_is_long)
                return;

            const auto new_capacity = sizeof(Char) * std::max(new_length, _length) + 1; //_length + 1 for null terminator
            const auto new_buffer = (Char *) _allocator.allocate(new_capacity);
            memcpy(new_buffer, _data._short._buffer, std::min(short_string_capacity, _length) * sizeof(Char));
            new_buffer[_length] = 0;

            _data._long._buffer = new_buffer;
            _data._long._capacity = new_capacity;
            _is_long = true;
        }

        void _convert_to_short()
        {
            if (!_is_long)
                return;

            const auto buffer = _data._long._buffer;
            const auto capacity = _data._long._capacity;

            memcpy(_data._short._buffer, buffer, std::min(short_string_capacity, _length) * sizeof(Char));
            _data._short._buffer[_length] = 0;

            _allocator.free(buffer);
            _is_long = false;
        }
    };

    using string = mos::mos_basic_string<char, mos::default_allocator>;
} // namespace mos
