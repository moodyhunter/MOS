// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <cstddef>
#include <mos/default_allocator.hpp>
#include <mos_stdlib.hpp>

namespace mos
{
    template<typename TItem>
    class vector
    {
        static_assert(!std::is_void_v<TItem>, "doesn't make sense");

      public:
        vector() = default;
        vector(std::initializer_list<TItem> list)
        {
            reserve(list.size());
            for (const auto &item : list)
            {
                new (&at(m_size)) TItem(item);
                ++m_size;
            }
        }

        vector(const vector &other)
        {
            *this = other;
        }

        vector &operator=(const vector &other)
        {
            if (this != &other)
            {
                clear();
                for (size_t i = 0; i < other.m_size; ++i)
                    push_back(other.at(i));
            }
            return *this;
        }

        vector(vector &&other) noexcept : m_storage(other.m_storage), m_size(other.m_size), m_capacity(other.m_capacity)
        {
            other.m_storage = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        }

        vector &operator=(vector &&other) noexcept
        {
            if (this != &other)
            {
                clear();
                m_storage = other.m_storage;
                m_size = other.m_size;
                m_capacity = other.m_capacity;

                other.m_storage = nullptr;
                other.m_size = 0;
                other.m_capacity = 0;
            }
            return *this;
        }

        ~vector() noexcept
        {
            clear();
        }

      private:
        void *m_storage = nullptr;
        size_t m_size = 0;
        size_t m_capacity = 0;

      public:
        auto &operator[](size_t index) const noexcept
        {
            return at(index);
        }

        auto &at(this auto &Self, size_t index) noexcept
        {
            const auto storage_item = reinterpret_cast<TItem *>(Self.m_storage);
            return storage_item[index];
        }

        auto data(this auto &Self) noexcept
        {
            if constexpr (std::is_const_v<decltype(Self)>)
                return reinterpret_cast<const TItem *>(Self.m_storage);
            else
                return reinterpret_cast<TItem *>(Self.m_storage);
        }

        auto size() const noexcept
        {
            return m_size;
        }

        auto capacity() const noexcept
        {
            return m_capacity;
        }

        auto empty() const noexcept
        {
            return m_size == 0;
        }

        auto reserve(size_t newSize) noexcept
        {
            newSize = GetNewCapacityForSize(newSize);
            if (newSize > m_capacity)
            {
                TItem *newStorage = reinterpret_cast<TItem *>(kcalloc<char>(newSize * sizeof(TItem)));

                for (size_t i = 0; i < m_size; ++i)
                    newStorage[i] = std::move(at(i));

                kfree(m_storage);
                m_storage = newStorage;
                m_capacity = newSize;
            }
        }

        auto resize(size_t new_size) noexcept
        {
            if (new_size > m_capacity)
                reserve(new_size);

            for (size_t i = m_size; i < new_size; ++i)
                new (std::addressof(at(m_size))) TItem();

            for (size_t i = new_size; i < m_size; ++i)
                at(i).~TItem();

            m_size = new_size;
        }

        auto push_back(const TItem &value) noexcept
        {
            if (m_size == m_capacity)
                reserve(m_capacity + 1);
            new (std::addressof(at(m_size))) TItem(value);
            ++m_size;
        }

        auto push_back(TItem &&value) noexcept
        {
            if (m_size == m_capacity)
                reserve(m_capacity + 1);
            new (std::addressof(at(m_size))) TItem(std::move(value));
            ++m_size;
        }

        auto pop_back() noexcept
        {
            if (m_size > 0)
            {
                --m_size;
                at(m_size).~TItem();
            }
        }

        auto clear() noexcept
        {
            if (m_storage && m_size)
            {
                while (!empty())
                    pop_back();

                kfree(m_storage);
                m_size = 0;
                m_storage = nullptr;
                m_capacity = 0;
            }
        }

      private:
        static constexpr size_t GetNewCapacityForSize(size_t newSize) noexcept
        {
            size_t cap = 1;
            while (cap < newSize)
                cap *= 2;
            return cap;
        }

      private:
        template<typename TPointer, typename TReference>
        class base_iterator
        {
          private:
            TPointer m_storage;
            size_t m_index;

          public:
            explicit base_iterator(void *storage, size_t index) noexcept : m_storage(reinterpret_cast<TPointer>(storage)), m_index(index) {};

            bool operator!=(const base_iterator &other) const noexcept
            {
                return m_index != other.m_index;
            }

            base_iterator &operator++() noexcept
            {
                ++m_index;
                return *this;
            }

            TReference operator*() const noexcept
            {
                return m_storage[m_index];
            }
        };

      private:
        using iterator = base_iterator<TItem *, TItem &>;
        using const_iterator = base_iterator<const TItem *, const TItem &>;

      public:
        auto begin() noexcept
        {
            return iterator(m_storage, 0);
        }

        auto end() noexcept
        {
            return iterator(m_storage, m_size);
        }

        auto begin() const noexcept
        {
            return const_iterator(m_storage, 0);
        }

        auto end() const noexcept
        {
            return const_iterator(m_storage, m_size);
        }
    };
} // namespace mos
