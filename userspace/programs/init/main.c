// SPDX-License-Identifier: GPL-3.0-or-later

#include "argparse/libargparse.h"
#include "parser.h"

#include <mos/filesystem/fs_types.h>
#include <mos/syscall/usermode.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static init_config_t *config;

static bool start_device_drivers(void)
{
    size_t num_drivers;
    const char **drivers = config_get_all(config, "driver", &num_drivers);
    if (!drivers)
        return false;

    for (size_t i = 0; i < num_drivers; i++)
    {
        const char *driver = drivers[i];

        char *dup = strdup(driver);
        char *driver_path = strtok(dup, " ");
        char *driver_args = strtok(NULL, " ");

        if (!driver_path)
            return false; // invalid options

        size_t driver_args_count = 0;
        const char **driver_argv = NULL;

        if (driver_args)
        {
            char *arg = strtok(driver_args, " ");
            while (arg)
            {
                driver_args_count++;
                driver_argv = realloc(driver_argv, driver_args_count * sizeof(char *));
                driver_argv[driver_args_count - 1] = arg;
                arg = strtok(NULL, " ");
            }
        }

        pid_t driver_pid = syscall_spawn(driver_path, driver_args_count, driver_argv);
        if (driver_pid <= 0)
            return false;
    }

    return true;
}

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

static bool create_directories(void)
{
    size_t num_dirs;
    const char **dirs = config_get_all(config, "mkdir", &num_dirs);
    if (!dirs)
        return false;

    for (size_t i = 0; i < num_dirs; i++)
    {
        const char *dir = dirs[i];
        if (!syscall_vfs_mkdir(dir))
            return false;
    }

    return true;
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

static const argparse_arg_t longopts[] = {
    { "config", 'C', ARGPARSE_REQUIRED }, // configuration file, default: /initrd/config/init.conf
    { "shell", 'S', ARGPARSE_REQUIRED },  // the shell or another program to launch
    { 0 },
};

int main(int argc, const char *argv[])
{
    MOS_UNUSED(argc);

    const char *config_file = "/initrd/config/init.conf";
    const char *shell = "/initrd/programs/mossh";
    argparse_state_t options;
    argparse_init(&options, argv);
    while (true)
    {
        const int option = argparse_long(&options, longopts, NULL);
        if (option == -1)
            break;

        switch (option)
        {
            case 'C': config_file = options.optarg; break;
            case 'S': shell = options.optarg; break;
            default: printf("Unknown option: %c\n", option);
        }
    }

    config = config_parse_file(config_file);
    if (!config)
        return DYN_ERROR_CODE;

    if (!create_directories())
        return DYN_ERROR_CODE;

    if (!mount_filesystems())
        return DYN_ERROR_CODE;

    pid_t dm_pid = start_device_manager();
    if (dm_pid <= 0)
        return DYN_ERROR_CODE;

    bool drivers_started = start_device_drivers();
    if (!drivers_started)
        return DYN_ERROR_CODE;

    // start the shell
    const char **shell_argv = NULL;
    int shell_argc = 0;

    const char *arg;
    argparse_init(&options, argv); // reset the options
    while ((arg = argparse_arg(&options)))
    {
        shell_argc++;
        shell_argv = realloc(shell_argv, shell_argc * sizeof(char *));
        shell_argv[shell_argc - 1] = arg;
    }

    // TODO: use exec() ?
    pid_t shell_pid = syscall_spawn(shell, shell_argc, shell_argv);
    if (shell_pid <= 0)
        return DYN_ERROR_CODE;

    syscall_wait_for_process(shell_pid);
    return 0;
}
