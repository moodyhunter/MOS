// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/memory.h"
#include "mos/filesystem/fs_types.h"
#include "mos/syscall/usermode.h"
#include "parser.h"

static init_config_t *config;

static pid_t start_device_manager(void)
{
    const char *dm_path = config_get(config, "device_manager.path");
    if (!dm_path)
        dm_path = "/bin/device_manager";

    size_t dm_args_count;
    const char **dm_args = config_get_all(config, "device_manager.args", &dm_args_count);

    // start the device manager
    return syscall_spawn(dm_path, dm_args_count, dm_args); // TODO: check if the dm_args are valid
}

int main(int argc, char *argv[])
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);

    file_stat_t stat;
    if (!syscall_file_stat("/assets/symlink-rel", &stat))
        return 1;

    char buf[PATH_MAX] = { 0 };
    size_t size = syscall_vfs_readlink("/assets/symlink-abs", buf, PATH_MAX);
    if (size == 0)
        return 1;

    if (!syscall_file_stat(buf, &stat))
        return 1;

    fd_t fd = syscall_file_open("/assets/symlink-rel", FILE_OPEN_READ);
    if (fd <= 0)
        return 2;

    // We don't have a console yet, so printing to stdout will not work.
    fd_t config_fd = syscall_file_open("/assets/config/init.conf", FILE_OPEN_READ);

    if (config_fd <= 0)
        return 1;

    config = config_parse_file(config_fd);

    if (!config)
        return 2;

    syscall_io_close(config_fd);

    pid_t dm_pid = start_device_manager();
    if (dm_pid <= 0)
        return 3;

    while (1)
        ;
    return 0;
}
