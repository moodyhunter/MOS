// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/platform/platform.h>
#include <mos/types.h>

void ipi_init(void);
void ipi_send(u8 target, ipi_type_t type);
void ipi_send_all(ipi_type_t type);
void ipi_do_handle(ipi_type_t type);
