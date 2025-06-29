// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/filesystem/vfs_types.hpp"

#include <mos/hashmap.hpp>
#include <mos/string.hpp>

namespace mos::kmods
{
    struct ModuleELFInfo;

    class Module : public NamedType<"Module">
    {
      public:
        using ExportedFunction = long (*)(void *arg, size_t argSize);
        explicit Module(const mos::string &path, inode_t *inode);

      public:
        void ExportFunction(const mos::string &name, ExportedFunction handler);
        ValueResult<long> TryCall(const mos::string &name, void *arg, size_t argSize);

        const mos::string &GetName() const
        {
            return name;
        }

      private:
        mos::string name; // kernel module name
        mos::HashMap<mos::string, ExportedFunction> exported_functions;

      private:
        inode_t *inode;                 // inode of the module file, with reference held
        ptr<ModuleELFInfo> module_info; // ELF module information

        friend PtrResult<ptr<Module>> LoadModule(const mos::string &path);
    };

    inline mos::HashMap<mos::string, ptr<Module>> kmod_map;

    /**
     * @brief Load a kernel module from the given path.
     *
     * @param path
     * @return KernelModule&
     */
    PtrResult<ptr<Module>> LoadModule(const mos::string &path);

    /**
     * @brief Get a kernel module by its name.
     *
     * @param name
     * @return std::optional<KernelModule &>
     */
    PtrResult<ptr<Module>> GetModule(const mos::string &name);
} // namespace mos::kmods
