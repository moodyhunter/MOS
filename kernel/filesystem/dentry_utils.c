// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/dentry.h"
#include "mos/filesystem/mount.h"
#include "mos/filesystem/vfs.h"
#include "mos/filesystem/vfs_types.h"
#include "mos/printk.h"

#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <mos_string.h>

dentry_t *dentry_ref(dentry_t *dentry)
{
    MOS_ASSERT(dentry);
    MOS_ASSERT(dentry->inode); // one cannot refcount a dentry without an inode
    dentry->refcount++;
    pr_dinfo2(dcache_ref, "dentry %p '%s' increased refcount to %zu", (void *) dentry, dentry_name(dentry), dentry->refcount);
    return dentry;
}

dentry_t *dentry_ref_up_to(dentry_t *dentry, dentry_t *root)
{
    pr_dinfo2(dcache_ref, "dentry_ref_up_to(%p '%s', %p '%s')", (void *) dentry, dentry_name(dentry), (void *) root, dentry_name(root));
    for (dentry_t *cur = dentry; cur != root; cur = dentry_parent(cur))
    {
        dentry_ref(cur);
        if (cur->name == NULL)
        {
            cur = dentry_root_get_mountpoint(cur);
            dentry_ref(cur);
        }
    }

    dentry_ref(root); // it wasn't refcounted in the loop

    pr_dinfo2(dcache_ref, "...done");
    return dentry;
}

/**
 * @brief Decrease the refcount of ONE SINGLE dentry, including (if it's a mountpoint) the mountpoint dentry
 *
 * @param dentry The dentry to decrease the refcount of
 * @return true if the refcount was decreased, false if the refcount was already 0
 */
__nodiscard bool dentry_unref_one_norelease(dentry_t *dentry)
{
    if (dentry == NULL)
        return false;

    if (dentry->refcount == 0)
    {
        mos_warn("dentry refcount is already 0");
        return false;
    }

    dentry->refcount--;
    pr_dinfo2(dcache_ref, "dentry %p '%s' decreased refcount to %zu", (void *) dentry, dentry_name(dentry), dentry->refcount);

    if (dentry->name == NULL && dentry != root_dentry)
    {
        dentry_t *mountpoint = dentry_root_get_mountpoint(dentry);
        if (!mountpoint)
            goto done;
        mountpoint->refcount--;
        pr_dinfo2(dcache_ref, "  mountpoint %p '%s' decreased mountpoint refcount to %zu", (void *) mountpoint, dentry_name(mountpoint), mountpoint->refcount);
    }
done:
    return true;
}

void dentry_dump_refstat(const dentry_t *dentry, dump_refstat_receiver_t *receiver, void *receiver_data)
{
    if (dentry == NULL)
        return;
    static int depth = 0;

    receiver(depth, dentry, false, receiver_data);

    if (dentry->is_mountpoint)
    {
        dentry = dentry_get_mount(dentry)->root;
        receiver(depth, dentry, true, receiver_data);
    }

    depth++;
    tree_foreach_child(dentry_t, child, dentry)
    {
        dentry_dump_refstat(child, receiver, receiver_data);
    }
    depth--;
}

void dentry_check_refstat(const dentry_t *dentry)
{
    size_t expected_refcount = 0;

    if (dentry != root_dentry)
    {
        if (dentry->is_mountpoint)
            expected_refcount++; // the mountpoint itself

        if (dentry->name == NULL)
            expected_refcount++; // the mounted root dentry
    }
    else
    {
        expected_refcount++; // the root dentry should only has one reference
    }

    tree_foreach_child(dentry_t, child, dentry)
    {
        expected_refcount += child->refcount;
    }

    if (dentry->refcount < expected_refcount)
    {
        mos_warn("dentry %p refcount %zu is less than expected refcount %zu", (void *) dentry, dentry->refcount, expected_refcount);
        tree_foreach_child(dentry_t, child, dentry)
        {
            pr_warn("  child %p '%s' has %zu references", (void *) child, dentry_name(child), child->refcount);
        }
        mos_panic("don't know how to handle this");
    }
    else if (dentry->refcount - expected_refcount)
    {
        pr_dinfo2(dcache_ref, "  dentry %p '%s' has %zu direct references", (void *) dentry, dentry_name(dentry), dentry->refcount - expected_refcount);
    }
}

void dentry_try_release(dentry_t *dentry)
{
    MOS_ASSERT(dentry->refcount == 0);

    const bool can_release = dentry->inode == NULL && list_is_empty(&tree_node(dentry)->children);
    if (can_release)
    {
        list_remove(&dentry->tree_node);
        if (dentry->name)
            kfree(dentry->name);
        kfree(dentry);
    }
}

void dentry_unref(dentry_t *dentry)
{
    if (!dentry_unref_one_norelease(dentry))
        return;

    dentry_check_refstat(dentry);
    dentry_unref(dentry_parent(dentry));

    if (dentry->refcount == 0)
        dentry_try_release(dentry);
}

ssize_t dentry_path(dentry_t *dentry, dentry_t *root, char *buf, size_t size)
{
    if (dentry == NULL)
        return 0;

    if (size < 2)
        return -1;

    if (dentry == root)
    {
        buf[0] = '/';
        buf[1] = '\0';
        return 1;
    }

    if (dentry->name == NULL)
        dentry = dentry_root_get_mountpoint(dentry);

    char *path = strdup(dentry->name);

    for (dentry_t *current = dentry_parent(dentry); current != root; current = dentry_parent(current))
    {
        if (current->name == NULL)
            current = dentry_root_get_mountpoint(current);

        char *newpath = kmalloc(strlen(current->name) + 1 + strlen(path) + 1);
        strcpy(newpath, current->name);
        strcat(newpath, "/");
        strcat(newpath, path);
        kfree(path);
        path = newpath;
    }

    if (strlen(path) + 1 > size)
        return -1;

    const size_t real_size = snprintf(buf, size, "/%s", path);
    kfree(path);

    return real_size;
}
