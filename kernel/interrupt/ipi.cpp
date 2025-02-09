// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/assert.hpp"

#include <mos/interrupt/ipi.hpp>
#include <mos/platform/platform.hpp>
#include <mos/syslog/printk.hpp>
#include <mos/types.hpp>

#if MOS_CONFIG(MOS_SMP)
#include "mos/tasks/schedule.hpp"

static void ipi_handler_halt(ipi_type_t type)
{
    MOS_UNUSED(type);
    pr_info("halt IPI received");
    platform_halt_cpu();
}

static void ipi_handler_invalidate_tlb(ipi_type_t type)
{
    MOS_UNUSED(type);
    pr_dinfo2(ipi, "Received invalidate TLB IPI");
    platform_invalidate_tlb(0);
}

static void ipi_handler_reschedule(ipi_type_t type)
{
    MOS_UNUSED(type);
    pr_dinfo2(ipi, "Received reschedule IPI");
    spinlock_acquire(&current_thread->state_lock);
    reschedule();
}

#define IPI_ENTRY(_type, _handler) [_type] = { .handle = _handler, .nr = PER_CPU_VAR_INIT }

static struct
{
    void (*handle)(ipi_type_t type);
    PER_CPU_DECLARE(size_t, nr);
} ipi_handlers[IPI_TYPE_MAX] = {
    IPI_ENTRY(IPI_TYPE_HALT, ipi_handler_halt),
    IPI_ENTRY(IPI_TYPE_INVALIDATE_TLB, ipi_handler_invalidate_tlb),
    IPI_ENTRY(IPI_TYPE_RESCHEDULE, ipi_handler_reschedule),
};

void ipi_send(u8 target, ipi_type_t type)
{
    pr_dinfo2(ipi, "Sending IPI to %d of type %d", target, type);
    platform_ipi_send(target, type);
}

void ipi_send_all(ipi_type_t type)
{
    pr_dinfo2(ipi, "Sending IPI to all of type %d", type);
    platform_ipi_send(TARGET_CPU_ALL, type);
}

void ipi_do_handle(ipi_type_t type)
{
    pr_dinfo2(ipi, "Handling IPI of type %d", type);

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

    (*per_cpu(ipi_handlers[type].nr))++;
    ipi_handlers[type].handle(type);
}
#else
// clang-format off
#define STUB_FUNCTION(func, ...) void func(__VA_ARGS__){}
#define STUB_FUNCTION_UNREACHABLE(func, ...) void func(__VA_ARGS__){ MOS_UNREACHABLE(); }
// clang-format on

STUB_FUNCTION(ipi_send, u8 __maybe_unused target, ipi_type_t __maybe_unused type)
STUB_FUNCTION(ipi_send_all, ipi_type_t __maybe_unused type)
STUB_FUNCTION(ipi_init, )
STUB_FUNCTION_UNREACHABLE(ipi_do_handle, ipi_type_t __maybe_unused type)
#endif
