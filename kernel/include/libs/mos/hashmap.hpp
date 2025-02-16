// SPDX-License-Identifier: MIT
// Adapted from https://github.com/managarm/frigg

#pragma once

#include "mos/assert.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <mos/allocator.hpp>
#include <mos/type_utils.hpp>
#include <mos_stdlib.hpp>
#include <optional>

namespace mos
{
    template<typename Key, typename Value, typename TAllocator = mos::default_allocator>
    class HashMap
    {
      public:
        typedef std::tuple<const Key, Value> entry_type;

      private:
        struct chain : mos::NamedType<"HashMap.Chain">
        {
            entry_type entry;
            chain *next;
            explicit chain(const Key &new_key, const Value &new_value) : entry(new_key, new_value), next(nullptr) {};
            explicit chain(const Key &new_key, Value &&new_value) : entry(new_key, std::move(new_value)), next(nullptr) {};
        };

      public:
        class iterator
        {
            friend class HashMap;

          public:
            iterator &operator++()
            {
                MOS_ASSERT(item);
                item = item->next;
                if (item)
                    return *this;

                MOS_ASSERT(bucket < map->_capacity);
                while (true)
                {
                    bucket++;
                    if (bucket == map->_capacity)
                        break;
                    item = map->_table[bucket];
                    if (item)
                        break;
                }

                return *this;
            }

            bool operator==(const iterator &other) const
            {
                return (bucket == other.bucket) && (item == other.item);
            }

            entry_type &operator*()
            {
                return item->entry;
            }
            entry_type *operator->()
            {
                return &item->entry;
            }

            operator bool()
            {
                return item != nullptr;
            }

          private:
            iterator(HashMap *map, size_t bucket, chain *item) : map(map), item(item), bucket(bucket) {};
            HashMap *map;
            chain *item;
            size_t bucket;
        };

        class const_iterator
        {
            friend class HashMap;

          public:
            const_iterator &operator++()
            {
                MOS_ASSERT(item);
                item = item->next;
                if (item)
                    return *this;

                MOS_ASSERT(bucket < map->_capacity);
                while (true)
                {
                    bucket++;
                    if (bucket == map->_capacity)
                        break;
                    item = map->_table[bucket];
                    if (item)
                        break;
                }

                return *this;
            }

            bool operator==(const const_iterator &other) const
            {
                return (bucket == other.bucket) && (item == other.item);
            }

            const entry_type &operator*() const
            {
                return item->entry;
            }

            const entry_type *operator->() const
            {
                return &item->entry;
            }

            operator bool() const
            {
                return item != nullptr;
            }

          private:
            const_iterator(const HashMap *map, size_t bucket, const chain *item) : map(map), item(item), bucket(bucket) {};
            const HashMap *map;
            const chain *item;
            size_t bucket;
        };

        constexpr HashMap(TAllocator allocator = TAllocator());
        HashMap(std::initializer_list<entry_type> init, TAllocator allocator = TAllocator());
        HashMap(const HashMap &) = delete;

        ~HashMap();

        void insert(const Key &key, const Value &value);
        void insert(const Key &key, Value &&value);
        Value &operator[](const Key &key);

        bool empty()
        {
            return !_size;
        }

        iterator end()
        {
            return iterator(this, _capacity, nullptr);
        }

        iterator find(const Key &key)
        {
            if (!_size)
                return end();

            const auto bucket = (std::hash<Key>{}(key) % _capacity);
            for (chain *item = _table[bucket]; item != nullptr; item = item->next)
            {
                if (std::get<0>(item->entry) == key)
                    return iterator(this, bucket, item);
            }

            return end();
        }

        iterator begin()
        {
            if (!_size)
                return iterator(this, _capacity, nullptr);

            for (size_t bucket = 0; bucket < _capacity; bucket++)
            {
                if (_table[bucket])
                    return iterator(this, bucket, _table[bucket]);
            }

            MOS_ASSERT(!"hash_map corrupted");
            MOS_UNREACHABLE();
        }

        const_iterator end() const
        {
            return const_iterator(this, _capacity, nullptr);
        }

        const_iterator find(const Key &key) const
        {
            if (!_size)
                return end();

            const auto bucket = (std::hash<Key>{}(key)) % _capacity;
            for (const chain *item = _table[bucket]; item != nullptr; item = item->next)
            {
                if (std::get<0>(item->entry) == key)
                    return const_iterator(this, bucket, item);
            }

            return end();
        }

        std::optional<Value *> get(const Key &key);
        std::optional<Value> remove(const Key &key);

        size_t size() const
        {
            return _size;
        }

      private:
        void rehash();

        TAllocator _allocator;
        chain **_table;
        size_t _capacity;
        size_t _size;
    };

    template<typename Key, typename Value, typename Allocator>
    constexpr HashMap<Key, Value, Allocator>::HashMap(Allocator allocator) : _allocator(std::move(allocator)), _table(nullptr), _capacity(0), _size(0){};

    template<typename Key, typename Value, typename Allocator>
    HashMap<Key, Value, Allocator>::HashMap(std::initializer_list<entry_type> init, Allocator allocator)
        : _allocator(std::move(allocator)), _table(nullptr), _capacity(0), _size(0)
    {
        /* TODO: we know the size so we don't have to keep rehashing?? */
        for (auto &[key, value] : init)
            insert(key, value);
    }

    template<typename Key, typename Value, typename Allocator>
    HashMap<Key, Value, Allocator>::~HashMap()
    {
        for (size_t i = 0; i < _capacity; i++)
        {
            chain *item = _table[i];
            while (item != nullptr)
            {
                chain *next = item->next;
                delete item;
                item = next;
            }
        }

        _allocator.free(_table, sizeof(chain *) * _capacity);
    }

    template<typename Key, typename Value, typename Allocator>
    void HashMap<Key, Value, Allocator>::insert(const Key &key, const Value &value)
    {
        if (_size >= _capacity)
            rehash();

        MOS_ASSERT(_capacity > 0);
        const auto bucket = (std::hash<Key>{}(key)) % _capacity;

        auto item = mos::create<chain>(key, value);
        item->next = _table[bucket];
        _table[bucket] = item;
        _size++;
    }

    template<typename Key, typename Value, typename Allocator>
    void HashMap<Key, Value, Allocator>::insert(const Key &key, Value &&value)
    {
        if (_size >= _capacity)
            rehash();

        MOS_ASSERT(_capacity > 0);
        const auto bucket = (std::hash<Key>{}(key)) % _capacity;

        auto item = mos::create<chain>(key, std::move(value));
        item->next = _table[bucket];
        _table[bucket] = item;
        _size++;
    }

    template<typename Key, typename Value, typename Allocator>
    Value &HashMap<Key, Value, Allocator>::operator[](const Key &key)
    {
        /* empty map case */
        if (_size == 0)
        {
            rehash();
            const auto bucket = (std::hash<Key>{}(key)) % _capacity;
            auto item = mos::create<chain>(key, Value{});
            item->next = _table[bucket];
            _table[bucket] = item;
            _size++;
        }

        const auto bucket = (std::hash<Key>{}(key)) % _capacity;
        for (chain *item = _table[bucket]; item != nullptr; item = item->next)
        {
            if (std::get<0>(item->entry) == key)
                return std::get<1>(item->entry);
        }

        if (_size >= _capacity)
            rehash();

        auto item = mos::create<chain>(key, Value{});
        item->next = _table[bucket];
        _table[bucket] = item;
        _size++;
        return std::get<1>(item->entry);
    }

    template<typename Key, typename Value, typename Allocator>
    std::optional<Value *> HashMap<Key, Value, Allocator>::get(const Key &key)
    {
        if (_size == 0)
            return std::nullopt;

        const auto bucket = (std::hash<Key>{}(key)) % _capacity;

        for (chain *item = _table[bucket]; item != nullptr; item = item->next)
        {
            if (std::get<0>(item->entry) == key)
                return &std::get<1>(item->entry);
        }

        return std::nullopt;
    }

    template<typename Key, typename Value, typename Allocator>
    std::optional<Value> HashMap<Key, Value, Allocator>::remove(const Key &key)
    {
        if (_size == 0)
            return std::nullopt;

        const auto bucket = (std::hash<Key>{}(key)) % _capacity;

        chain *previous = nullptr;
        for (chain *item = _table[bucket]; item != nullptr; item = item->next)
        {
            if (std::get<0>(item->entry) == key)
            {
                Value value = std::move(std::get<1>(item->entry));

                if (previous == nullptr)
                    _table[bucket] = item->next;
                else
                    previous->next = item->next;
                delete item;
                _size--;
                return value;
            }

            previous = item;
        }

        return std::nullopt;
    }

    template<typename Key, typename Value, typename Allocator>
    void HashMap<Key, Value, Allocator>::rehash()
    {
        const size_t new_capacity = std::max(2 * _size, 10lu);

        const auto new_table = kcalloc<chain *>(new_capacity);
        for (size_t i = 0; i < new_capacity; i++)
            new_table[i] = nullptr;

        for (size_t i = 0; i < _capacity; i++)
        {
            auto item = _table[i];
            while (item != nullptr)
            {
                const auto &key = std::get<0>(item->entry);
                const auto bucket = (std::hash<Key>{}(key)) % new_capacity;

                const auto next = item->next;
                item->next = new_table[bucket];
                new_table[bucket] = item;
                item = next;
            }
        }

        _allocator.free(_table, sizeof(chain *) * _capacity);
        _table = new_table;
        _capacity = new_capacity;
    }
} // namespace mos
