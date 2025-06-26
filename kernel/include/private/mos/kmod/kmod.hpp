// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/filesystem/vfs_types.hpp"

#include <mos/hashmap.hpp>
#include <mos/refcount.hpp>
#include <mos/string.hpp>

namespace mos::kmods
{
    struct KernelModule : RefCount, NamedType<"KernelModule">
    {
        explicit KernelModule(const mos::string &path, inode_t *inode);

        mos::string name; // kernel module name
        inode_t *inode;   // inode of the module file, with reference held
    };

    inline mos::HashMap<mos::string, KernelModule *> kmod_map;

    /**
     * @brief Load a kernel module from the given path.
     *
     * @param path
     * @return KernelModule&
     */
    PtrResult<KernelModule> load(const mos::string &path);

    /**
     * @brief Get a kernel module by its name.
     *
     * @param name
     * @return std::optional<KernelModule &>
     */
    PtrResult<KernelModule> get(const mos::string &name);
} // namespace mos::kmods
