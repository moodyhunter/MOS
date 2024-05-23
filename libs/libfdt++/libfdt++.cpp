// SPDX-License-Identifier: GPL-3.0-or-later

#include "libfdt++.hpp"

#include <libfdt.h>

unsigned int dt_root_addr_cells = 2;
unsigned int dt_root_size_cells = 1;

static int pair_bytes()
{
    return (dt_root_addr_cells + dt_root_size_cells) * sizeof(u32);
}

/* Helper to read a big number; size is in cells (not bytes) */
// Adapted from Linux kernel's of_read_number()
static inline u64 of_read_number(const u32 *cell, int size)
{
    u64 r = 0;
    for (; size--; cell++)
        r = (r << 32) | be32_to_cpu(*cell);
    return r;
}

bool dt_reg::verify_validity() const
{
    if (!prop.len())
        return true; // zero-length reg property is valid

    if (prop.len() % pair_bytes())
        return false; // invalid length (total size is not a multiple of pair size)

    return true;
}

std::pair<ptr_t, size_t> dt_reg::iterator::operator*() const
{
    u32 *pcell = (u32 *) data;
    const ptr_t base = of_read_number(pcell, dt_root_addr_cells);
    const size_t size = of_read_number(pcell + dt_root_addr_cells, dt_root_size_cells);
    return std::make_pair(base, size);
}

dt_reg::iterator &dt_reg::iterator::operator++()
{
    data += pair_bytes();
    return *this;
}

dt_node::dt_node(const dt_root &root, const char *path) : m_root(root), m_offset(-1)
{
    m_offset = fdt_path_offset(root.fdt(), path);
}

const char *dt_node::get_name() const
{
    return fdt_get_name(m_root.fdt(), m_offset, nullptr);
}

dt_node::iterator &dt_node::iterator::operator++()
{
    iter_offset = fdt_next_subnode(node.m_root.fdt(), iter_offset);
    return *this;
}

dt_node::iterator dt_node::begin() const
{
    return iterator(*this, fdt_first_subnode(m_root.fdt(), m_offset));
}

dt_node::iterator dt_node::end() const
{
    return iterator(*this, -FDT_ERR_NOTFOUND);
}

dt_node::node_property_list::iterator dt_node::node_property_list::begin() const
{
    return iterator(this->node, fdt_first_property_offset(node.root().fdt(), node.offset()));
}

dt_node::node_property_list::iterator dt_node::node_property_list::end() const
{
    return iterator(this->node, -FDT_ERR_NOTFOUND);
}

dt_node::node_property_list::iterator &dt_node::node_property_list::iterator::operator++()
{
    poffset = fdt_next_property_offset(node.root().fdt(), poffset);
    return *this;
}

bool dt_node::has_property(const char *name) const
{
    return fdt_get_property(m_root.fdt(), m_offset, name, nullptr) != nullptr;
}

dt_property::dt_property(const dt_node &node, const char *name) : m_node(node), m_name(name)
{
    m_propdata = fdt_getprop(node.root().fdt(), node.offset(), name, &m_len);
}

dt_property::dt_property(const dt_node &node, int poffset) : m_node(node)
{
    m_propdata = fdt_getprop_by_offset(node.root().fdt(), poffset, &m_name, &m_len);
}

u32 dt_property::get_u32() const
{
    return fdt32_to_cpu(*(const u32 *) m_propdata);
}

u64 dt_property::get_u64() const
{
    return fdt64_to_cpu(*(const u64 *) m_propdata);
}

const char *dt_property::get_string() const
{
    return (const char *) m_propdata;
}
