// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/dentry.hpp"
#include "mos/filesystem/mount.hpp"
#include "mos/filesystem/vfs_types.hpp"
#include "mos/lib/sync/spinlock.hpp"

#include <algorithm>
#include <mos_stdio.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

using namespace mos::string_literals;

void dentry_try_release(ptr<dentry_t> dentry)
{
    const bool can_release = dentry->inode == nullptr && dentry->children.empty();
    dInfo<dcache> << fmt("releasing dentry {} '{}', can_release: {}", (void *) dentry.get(), dentry_name(dentry), can_release);
    if (const auto parent = dentry->parent; can_release && parent)
    {
        SpinLocker locker(&parent->lock);
        const auto it = std::remove_if(parent->children.begin(), parent->children.end(), [dentry](const ptr<dentry_t> &child) { return child == dentry; });
        parent->children.erase(it, parent->children.end());
        dentry->parent = nullptr;
    }
}

std::optional<mos::string> dentry_path(ptr<dentry_t> dentry, ptr<dentry_t> root)
{
    if (dentry == nullptr)
        return std::nullopt;

    if (dentry == root)
        return "/";

    if (dentry->name.empty())
        dentry = dentry_root_get_mountpoint(dentry);

    auto path = dentry->name;

    for (ptr<dentry_t> current = dentry->parent; current != root; current = current->parent)
    {
        if (current->name.empty())
            current = dentry_root_get_mountpoint(current);

        // root for other fs trees (memfd, etc.)
        if (current == nullptr)
            return ":/" + path;
        else
            path = current->name + "/" + path;
    }

    return "/" + path;
}
