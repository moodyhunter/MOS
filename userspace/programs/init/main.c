// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/memory.h"
#include "lib/string.h"
#include "libuserspace.h"
#include "mos/filesystem/fs_types.h"
#include "mos/syscall/usermode.h"
#include "parser.h"

static init_config_t *config;

static pid_t start_device_manager(void)
{
    const char *dm_path = config_get(config, "device_manager.path");
    if (!dm_path)
        dm_path = "/initrd/programs/device_manager";

    size_t dm_args_count;
    const char **dm_args = config_get_all(config, "device_manager.args", &dm_args_count);

    // start the device manager
    return syscall_spawn(dm_path, dm_args_count, dm_args); // TODO: check if the dm_args are valid
}

static bool mount_filesystems(void)
{
    size_t num_mounts;
    const char **mounts = config_get_all(config, "mount", &num_mounts);
    if (!mounts)
        return false;

    for (size_t i = 0; i < num_mounts; i++)
    {
        // format: <Device> <MountPoint> <Filesystem> <Options>
        const char *mount = mounts[i];

        char *dup = strdup(mount);
        char *device = strtok(dup, " ");
        char *mount_point = strtok(NULL, " ");
        char *filesystem = strtok(NULL, " ");
        char *options = strtok(NULL, " ");

        if (!device || !mount_point || !filesystem || !options)
            return false; // invalid options

#define remove_leading_trailing_spaces(str)                                                                                                                              \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        while (*str == ' ')                                                                                                                                              \
            str++;                                                                                                                                                       \
        while (str[strlen(str) - 1] == ' ')                                                                                                                              \
            str[strlen(str) - 1] = '\0';                                                                                                                                 \
    } while (0)

        remove_leading_trailing_spaces(device);
        remove_leading_trailing_spaces(mount_point);
        remove_leading_trailing_spaces(filesystem);
        remove_leading_trailing_spaces(options);

        if (!syscall_vfs_mount(device, mount_point, filesystem, options))
            return false;
    }

    return true;
}

#define DYN_ERROR_CODE (__COUNTER__ + 1)

int main(int argc, char *argv[])
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);

    config = config_parse_file("/initrd/config/init.conf");
    if (!config)
        return DYN_ERROR_CODE;

    if (!mount_filesystems())
        return DYN_ERROR_CODE;

    pid_t dm_pid = start_device_manager();
    if (dm_pid <= 0)
        return DYN_ERROR_CODE;

    const char *ls_path = "/ipc/";
    syscall_spawn("/initrd/programs/ls", 1, &ls_path);

    while (1)
        ;
    return 0;
}
