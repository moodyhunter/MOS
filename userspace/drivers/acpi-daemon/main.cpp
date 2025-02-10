// SPDX-License-Identifier: GPL-3.0-or-later

#include "uacpi/event.h"
#include "uacpi/namespace.h"
#include "uacpi/uacpi.h"
#include "uacpi/utilities.h"

#include <fcntl.h>
#include <iostream>
#include <mos/mos_global.h>
#include <mos/syscall/usermode.h>

fd_t mem_fd = 0;

int main(int argc, const char *argv[])
{
    std::cout << "ACPI Daemon for MOS" << std::endl;

    syscall_arch_syscall(X86_SYSCALL_IOPL_ENABLE, 0, 0, 0, 0);

    mem_fd = open("/sys/mem", O_RDWR);

    /*
     * Start with this as the first step of the initialization. This loads all
     * tables, brings the event subsystem online, and enters ACPI mode. We pass
     * in 0 as the flags as we don't want to override any default behavior for now.
     */
    uacpi_status ret = uacpi_initialize(0);
    if (uacpi_unlikely_error(ret))
    {
        std::cerr << "uacpi_initialize error: " << uacpi_status_to_string(ret) << std::endl;
        return -ENODEV;
    }

    /*
     * Load the AML namespace. This feeds DSDT and all SSDTs to the interpreter
     * for execution.
     */
    ret = uacpi_namespace_load();
    if (uacpi_unlikely_error(ret))
    {
        std::cerr << "uacpi_namespace_load error: " << uacpi_status_to_string(ret) << std::endl;
        return -ENODEV;
    }

    /*
     * Initialize the namespace. This calls all necessary _STA/_INI AML methods,
     * as well as _REG for registered operation region handlers.
     */
    ret = uacpi_namespace_initialize();
    if (uacpi_unlikely_error(ret))
    {
        std::cerr << "uacpi_namespace_initialize error: %s" << uacpi_status_to_string(ret) << std::endl;
        return -ENODEV;
    }

    /*
     * Tell uACPI that we have marked all GPEs we wanted for wake (even though we haven't
     * actually marked any, as we have no power management support right now). This is
     * needed to let uACPI enable all unmarked GPEs that have a corresponding AML handler.
     * These handlers are used by the firmware to dynamically execute AML code at runtime
     * to e.g. react to thermal events or device hotplug.
     */
    ret = uacpi_finalize_gpe_initialization();
    if (uacpi_unlikely_error(ret))
    {
        std::cerr << "uACPI GPE initialization error: " << uacpi_status_to_string(ret) << std::endl;
        return -ENODEV;
    }

    const auto acpi_init_one_device = [](void *user, uacpi_namespace_node *node, uacpi_u32 node_depth) -> uacpi_iteration_decision
    {
        uacpi_namespace_node_info *info;

        uacpi_status ret = uacpi_get_namespace_node_info(node, &info);

        if (uacpi_unlikely_error(ret))
        {
            const char *path = uacpi_namespace_node_generate_absolute_path(node);
            std::cerr << "unable to retrieve node " << path << ", " << uacpi_status_to_string(ret) << std::endl;
            uacpi_free_absolute_path(path);
            return UACPI_ITERATION_DECISION_CONTINUE;
        }

        if (info->type != UACPI_OBJECT_DEVICE)
        {
            // We probably don't care about anything but devices at this point
            uacpi_free_namespace_node_info(info);
            return UACPI_ITERATION_DECISION_CONTINUE;
        }

        if (info->flags & UACPI_NS_NODE_INFO_HAS_HID)
        {
            // Match the HID against every existing acpi_driver pnp id list
            std::cout << "HID: " << info->hid.value << std::endl;
        }

        if (info->flags & UACPI_NS_NODE_INFO_HAS_CID)
        {
            // Match the CID list against every existing acpi_driver pnp id list
            for (uacpi_u32 i = 0; i < info->cid.num_ids; i++)
            {
                std::cout << "CID: " << info->cid.ids[i].value << std::endl;
            }
        }

        uacpi_free_namespace_node_info(info);

        return UACPI_ITERATION_DECISION_CONTINUE;
    };

    uacpi_namespace_for_each_child(uacpi_namespace_root(), acpi_init_one_device, UACPI_NULL, UACPI_OBJECT_DEVICE_BIT, UACPI_MAX_DEPTH_ANY, UACPI_NULL);

    while (true)
        ;

    return 0;
}
