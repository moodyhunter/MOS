// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.h>
#include <mos/types.hpp>

/**
 * @brief The type of IPI to send
 *
 */
typedef enum
{
    IPI_TYPE_HALT = 0,       // halt the CPU
    IPI_TYPE_INVALIDATE_TLB, // TLB shootdown
    IPI_TYPE_RESCHEDULE,     // Reschedule
    IPI_TYPE_MAX,
} ipi_type_t;

MOS_STATIC_ASSERT(IPI_TYPE_MAX <= (u8) 0xFF, "IPI_TYPE_MAX must fit in a u8");

#define TARGET_CPU_ALL 0xFF

void ipi_send(u8 target, ipi_type_t type);
void ipi_send_all(ipi_type_t type);
void ipi_do_handle(ipi_type_t type);
