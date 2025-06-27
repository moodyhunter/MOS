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

        list(const list &other)
        {
            do_copy_from(other);
        }
        list(list &&other) noexcept
        {
            do_move_from(std::move(other));
        }
        list &operator=(const list &other)
        {
            if (this != &other)
                do_copy_from(other);
            return *this;
        }
        list &operator=(list &&other) noexcept
        {
            if (this != &other)
                do_move_from(std::move(other));
            return *this;
        }

        ~list()
        {
            clear();
        }

      private:
        void do_copy_from(const list &other)
        {
            clear();
            for (const auto &item : other)
                push_back(item);
        }

        void do_move_from(list &&other) noexcept
        {
            clear();
            head = other.head;
            tail = other.tail;
            other.head = nullptr;
            other.tail = nullptr;
        }

      public:
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
            friend class list;

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

            iterator operator++(int)
            {
                iterator temp = *this;
                current = current->next;
                return temp;
            }

            bool operator==(const iterator &other) const
            {
                return current == other.current;
            }

            bool operator!=(const iterator &other) const
            {
                return current != other.current;
            }

            T operator*() const
            {
                return current->value;
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
        friend class iterator;

      private:
        node *head = nullptr;
        node *tail = nullptr;
    };
} // namespace mos
