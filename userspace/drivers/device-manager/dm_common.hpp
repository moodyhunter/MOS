// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <libconfig/libconfig.h>
#include <mos/types.h>

bool start_load_drivers();
bool try_start_driver(u16 vendor, u16 device, u8 busid, u8 devid, u8 funcid, u64 mmio_base);

inline Config dm_config;
