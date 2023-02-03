// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/device/device_manager.h"

#include "mos/ipc/ipc.h"
#include "mos/printk.h"
#include "mos/tasks/kthread.h"

static io_t *server_io = NULL;

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
