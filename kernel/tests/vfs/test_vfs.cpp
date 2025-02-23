// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/dentry.hpp"
#include "mos/filesystem/vfs.hpp"
#include "test_engine_impl.h"

static void stat_receiver(int depth, const dentry_t *dentry, bool mountroot, void *data)
{
    MOS_UNUSED(data);
    pr_info2("%*s%s: %zu%s", depth * 4, "", dentry_name(dentry).c_str(), dentry->refcount.load(), mountroot ? " (mountroot)" : "");
}

MOS_TEST_DECL_PTEST(vfs_mount_test, "Mount %s in %s, with rootfs: %d", const char *fs, const char *mountpoint, bool rootfs)
{
    long ok = false;

    if (rootfs)
    {
        ok = vfs_mount("none", "/", "tmpfs", "").getErr();
        MOS_TEST_ASSERT(ok == 0, "failed to mount tmpfs on /");
    }

    ok = vfs_mkdir(mountpoint).getErr();
    MOS_TEST_ASSERT((ok == 0) == rootfs, "creating %s should %sbe successful", mountpoint, rootfs ? "" : "not");
    dentry_dump_refstat(root_dentry, stat_receiver, NULL);

    ok = vfs_mount("none", mountpoint, fs, "").getErr();
    MOS_TEST_ASSERT((ok == 0) == rootfs, "mounting %s on %s should %sbe successful", fs, mountpoint, rootfs ? "" : "not");
    dentry_dump_refstat(root_dentry, stat_receiver, NULL);

    ok = vfs_unmount(mountpoint);
    MOS_TEST_ASSERT((ok == 0) == rootfs, "unmounting /tmp should %sbe successful", rootfs ? "" : "not");
    dentry_dump_refstat(root_dentry, stat_receiver, NULL);

    ok = vfs_rmdir(mountpoint).getErr();
    MOS_TEST_ASSERT((ok == 0) == rootfs, "removing %s should %sbe successful", mountpoint, rootfs ? "" : "not");
    dentry_dump_refstat(root_dentry, stat_receiver, NULL);

    if (rootfs)
    {
        ok = vfs_unmount("/");
        MOS_TEST_ASSERT(ok == 0, "failed to unmount rootfs");
    }
}

MOS_TEST_PTEST_INSTANCE(vfs_mount_test, "tmpfs", "/tmp", false);
MOS_TEST_PTEST_INSTANCE(vfs_mount_test, "tmpfs", "/tmp", true);
