// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.h>
#include <utility>

extern unsigned int dt_root_addr_cells;
extern unsigned int dt_root_size_cells;

class dt_node;
class dt_root;
class dt_property;
class dt_reg;

class dt_property
{
  public:
    explicit dt_property(const dt_node &node, const char *name);
    explicit dt_property(const dt_node &node, int poffset);

  public:
    u32 get_u32() const;
    u64 get_u64() const;
    const char *get_string() const;

    // clang-format off
    int len() const { return m_len; }
    void *get_data() const { return (void *) m_propdata; }
    const char *get_name() const { return m_name; }
    operator bool() const { return m_propdata != nullptr; }
    // clang-format on

  private:
    const dt_node &m_node;
    const char *m_name;
    const void *m_propdata;
    int m_len;

    friend dt_reg;
};

/**
 * @brief Class to iterate over reg property cells
 *
 */
class dt_reg
{
    class iterator
    {
      public:
        explicit iterator(const dt_reg &reg, char *data) : reg(reg), data(data){};
        std::pair<ptr_t, size_t> operator*() const;
        iterator &operator++();

        // clang-format off
        bool operator!=(const iterator &other) const { return data != other.data; }
        // clang-format on

      private:
        const dt_reg &reg;
        const char *data;
    };

  public:
    dt_reg(const dt_property &prop) : prop(prop){};
    bool verify_validity() const;

    // clang-format off
    iterator begin() const { return iterator(*this, (char *) prop.m_propdata); }
    iterator end() const { return iterator(*this, (char *) prop.m_propdata + prop.m_len); }
    // clang-format on

  private:
    const dt_property &prop;
};

class dt_node
{
    class iterator
    {
      public:
        explicit iterator(const dt_node &node, int offset) : node(node), iter_offset(offset){};

      public:
        iterator &operator++();

        // clang-format off
        dt_node operator*() const { return dt_node(node.m_root, iter_offset); }
        bool operator!=(const iterator &other) const { return iter_offset != other.iter_offset; }
        // clang-format on

      private:
        const dt_node &node;
        int iter_offset;
    };

    class node_property_list
    {
        class iterator
        {
          public:
            explicit iterator(const dt_node &node, int offset) : node(node), poffset(offset){};

          public:
            iterator &operator++();

            // clang-format off
            dt_property operator*() const { return dt_property(node, poffset); }
            bool operator!=(const iterator &other) const { return poffset != other.poffset; }
            // clang-format on

          private:
            const dt_node &node;
            int poffset;
        };

      public:
        explicit node_property_list(const dt_node &node) : node(node){};

        iterator begin() const;
        iterator end() const;

      private:
        const dt_node &node;
    };

  public:
    explicit dt_node(const dt_root &root, unsigned long offset) : m_root(root), m_offset(offset){};
    explicit dt_node(const dt_root &root, const char *path);

  public:
    const char *get_name() const;

    // clang-format off
    long offset() const { return m_offset; }
    // clang-format on

    iterator begin() const;
    iterator end() const;

    bool has_property(const char *name) const;

    // clang-format off
    const dt_root &root() const { return m_root; }
    node_property_list properties() const { return node_property_list(*this); }
    dt_property get_property(const char *name) const { return dt_property(*this, name); }
    dt_property operator[](const char *name) const { return dt_property(*this, name); }
    // clang-format on

  private:
    const dt_root &m_root;
    unsigned long m_offset; // node offset of current node
};

class dt_root
{
  public:
    explicit dt_root(void *fdt) : _fdt(fdt), _root_node(*this, "/"){};

  public:
    // clang-format off
    void *fdt() const { return _fdt; }
    dt_node &rootnode() { return _root_node; }
    const dt_node &rootnode() const { return _root_node; }
    dt_node get_node(const char *path) const { return dt_node(*this, path); }
    // clang-format on

  private:
    void *_fdt;
    dt_node _root_node;
};
