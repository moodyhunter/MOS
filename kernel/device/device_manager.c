// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/device/device_manager.h"

#include "lib/structures/list.h"
#include "mos/device/block.h"
#include "mos/filesystem/fs_types.h"
#include "mos/ipc/ipc.h"
#include "mos/mm/kmalloc.h"
#include "mos/printk.h"
#include "mos/tasks/kthread.h"

static io_t *server_io = NULL;

dentry_t *devfs_mount(filesystem_t *fs, const char *dev, const char *options)
{
    // vfs_readlink(mount_point, mount_point->name, 0, 0)
    return 0;
}

// register a /dev filesystem
static filesystem_t devfs = {
    .name = "devfs",
    .ops =
        &(filesystem_ops_t){
            .mount = devfs_mount,
        },
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
