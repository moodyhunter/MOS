// SPDX-License-Identifier: GPL-3.0-or-later

#include "libuserspace.h"
#include "mos/device/device_manager.h"
#include "mos/syscall/usermode.h"

typedef struct dm_register_driver_request
{
    char name[32];
    char description[128];
    u32 major;
    u32 minor;
} dm_register_driver_request_t;

int main(int argc, char **argv)
{
    device_driver_t driver;
    return 0;
}
