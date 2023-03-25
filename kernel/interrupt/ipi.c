// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/interrupt/ipi.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/types.h>

void ipi_send(u8 target, ipi_type_t type)
{
    mos_debug(ipi, "Sending IPI to %d of type %d", target, type);
    platform_ipi_send(target, type);
}

void ipi_send_all(ipi_type_t type)
{
    mos_debug(ipi, "Sending IPI to all of type %d", type);
    platform_ipi_send(TARGET_CPU_ALL, type);
}

static void ipi_handler_halt(ipi_type_t type)
{
    MOS_UNUSED(type);
    mos_debug(ipi, "Received halt IPI");
    platform_halt_cpu();
}

static void ipi_handler_invalidate_tlb(ipi_type_t type)
{
    MOS_UNUSED(type);
    mos_debug(ipi, "Received invalidate TLB IPI");
    platform_invalidate_tlb();
}

static const struct
{
    ipi_type_t type;
    void (*handle)(ipi_type_t type);
} ipi_handlers[IPI_TYPE_MAX] = {
    { IPI_TYPE_HALT, ipi_handler_halt },
    { IPI_TYPE_INVALIDATE_TLB, ipi_handler_invalidate_tlb },
};

void ipi_init(void)
{
    ;
}

void ipi_do_handle(ipi_type_t type)
{
    mos_debug(ipi, "Handling IPI of type %d", type);

    if (type >= IPI_TYPE_MAX)
    {
        mos_warn("IPI type %d is out of range", type);
        return;
    }

    if (ipi_handlers[type].handle == NULL)
    {
        mos_warn("No handler for IPI type %d", type);
        return;
    }

    ipi_handlers[type].handle(type);
}
