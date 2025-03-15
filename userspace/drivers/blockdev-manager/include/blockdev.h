// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "proto/blockdev.service.h"

#include <librpc/macro_magic.h>

#define BLOCKDEV_MANAGER_RPC_SERVER_NAME "mos.blockdev-manager"

RPC_DECL_CPP_TYPE_NAMESPACE(blockdev, BLOCKDEVMANAGER_SERVICE_X);
RPC_DECL_CPP_TYPE_NAMESPACE(blockdev, BLOCKDEVDEVICE_SERVICE_X);
