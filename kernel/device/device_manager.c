// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/device/device_manager.h"

#include "lib/structures/list.h"
#include "mos/device/block.h"
#include "mos/filesystem/filesystem.h"
#include "mos/filesystem/mount.h"
#include "mos/ipc/ipc.h"
#include "mos/mm/kmalloc.h"
#include "mos/printk.h"
#include "mos/tasks/kthread.h"

static io_t *server_io = NULL;

bool devfs_mount(blockdev_t *dev, fsnode_t *mount_point)
{
    (void) dev;
    (void) mount_point;

    // vfs_readlink(mount_point, mount_point->name, 0, 0)

    pr_info("devfs mounted at %s", mount_point->name);
    return true;
}

// register a /dev filesystem
static filesystem_t devfs = {
    .name = "devfs",
    .op_mount = devfs_mount,
};

static void device_manager_thread(void *arg)
{
    (void) arg;
    MOS_ASSERT_X(server_io != NULL, "Device manager not initialized");

    while (true)
    {
        io_t *io = ipc_accept(server_io);
        MOS_UNUSED(io);
    }

    return;
}

void device_manager_init(void)
{
    MOS_ASSERT_X(server_io == NULL, "Device manager already initialized");
    server_io = ipc_create(MOS_DEVICE_MANAGER_SERVICE_NAME, 32);

    if (server_io == NULL)
        mos_panic("Unable to create IPC connection for device manager");

    kthread_create(device_manager_thread, 0, "device_manager");
    pr_info("Device manager initialized");
}
