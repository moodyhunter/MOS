// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <mos/allocator.hpp>

namespace mos
{
    template<typename T>
    class list
    {
      public:
        list() = default;
        list(const list &other) = default;
        list(list &&other) noexcept = default;
        list &operator=(const list &other) = default;
        list &operator=(list &&other) noexcept = default;
        ~list() = default;

        void push_back(const T &value)
        {
            if (head == nullptr)
            {
                head = mos::create<node>(value);
                tail = head;
            }
            else
            {
                tail->next = mos::create<node>(value);
                tail = tail->next;
            }
        }

        void push_front(const T &value)
        {
            if (head == nullptr)
            {
                head = mos::create<node>(value);
                tail = head;
            }
            else
            {
                node *new_head = mos::create<node>(value);
                new_head->next = head;
                head = new_head;
            }
        }

        void pop_back()
        {
            if (head == nullptr)
                return;

            if (head == tail)
            {
                delete head;
                head = nullptr;
                tail = nullptr;
                return;
            }

            node *current = head;
            while (current->next != tail)
                current = current->next;

            delete tail;
            tail = current;
            tail->next = nullptr;
        }

        void pop_front()
        {
            if (head == nullptr)
                return;

            if (head == tail)
            {
                delete head;
                head = nullptr;
                tail = nullptr;
                return;
            }

            node *new_head = head->next;
            delete head;
            head = new_head;
        }

        T &front()
        {
            return head->value;
        }

        T &back()
        {
            return tail->value;
        }

        bool empty() const
        {
            return head == nullptr;
        }

      private:
        struct node : mos::NamedType<"List.Node">
        {
            T value;
            node *next = nullptr;

            node(const T &value) : value(value) {};
        };

      public:
        class iterator
        {
          public:
            iterator(node *current) : current(current)
            {
            }

            T &operator*()
            {
                return current->value;
            }

            iterator &operator++()
            {
                current = current->next;
                return *this;
            }

            bool operator==(const iterator &other) const
            {
                return current == other.current;
            }

            bool operator!=(const iterator &other) const
            {
                return current != other.current;
            }

          private:
            node *current;
        };

        iterator begin()
        {
            return iterator(head);
        }

        iterator end()
        {
            return iterator(nullptr);
        }

        const iterator begin() const
        {
            return iterator(head);
        }

        const iterator end() const
        {
            return iterator(nullptr);
        }

        const iterator cbegin() const
        {
            return iterator(head);
        }

        const iterator cend() const
        {
            return iterator(nullptr);
        }

        iterator erase(iterator it)
        {
            if (it.current == head)
            {
                pop_front();
                return iterator(head);
            }

            node *current = head;
            while (current->next != it.current)
                current = current->next;

            current->next = it.current->next;
            delete it.current;
            return iterator(current->next);
        }

        void insert(iterator it, const T &value)
        {
            if (it.current == head)
            {
                push_front(value);
                return;
            }

            node *current = head;
            while (current->next != it.current)
                current = current->next;

            node *new_node = mos::create<node>(value);
            new_node->next = current->next;
            current->next = new_node;
        }

        void clear()
        {
            while (head != nullptr)
            {
                node *next = head->next;
                delete head;
                head = next;
            }
        }

      private:
        node *head = nullptr;
        node *tail = nullptr;
    };
} // namespace mos
