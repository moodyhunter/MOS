// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/lib/sync/spinlock.hpp"
#include "mos/x86/devices/port.hpp"
#include "uacpi/kernel_api.h"
#include "uacpi/platform/arch_helpers.h"
#include "uacpi/status.h"

#include <bits/posix/posix_stdio.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <pthread.h>
#include <string.h>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>
#include <vector>

extern fd_t mem_fd;

/*
 * Raw IO API, this is only used for accessing verified data from
 * "safe" code (aka not indirectly invoked by the AML interpreter),
 * e.g. programming FADT & FACS registers.
 *
 * NOTE:
 * 'byte_width' is ALWAYS one of 1, 2, 4, 8. You are NOT allowed to implement
 * this in terms of memcpy, as hardware expects accesses to be of the EXACT
 * width.
 * -------------------------------------------------------------------------
 */
uacpi_status uacpi_kernel_raw_memory_read(uacpi_phys_addr address, uacpi_u8 byte_width, uacpi_u64 *out_value)
{
    switch (byte_width)
    {
        case 1: *out_value = *(uacpi_u8 *) address; break;
        case 2: *out_value = *(uacpi_u16 *) address; break;
        case 4: *out_value = *(uacpi_u32 *) address; break;
        case 8: *out_value = *(uacpi_u64 *) address; break;
        default: return UACPI_STATUS_INVALID_ARGUMENT;
    };

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_raw_memory_write(uacpi_phys_addr address, uacpi_u8 byte_width, uacpi_u64 in_value)
{
    switch (byte_width)
    {
        case 1: *(uacpi_u8 *) address = in_value; break;
        case 2: *(uacpi_u16 *) address = in_value; break;
        case 4: *(uacpi_u32 *) address = in_value; break;
        case 8: *(uacpi_u64 *) address = in_value; break;
        default: return UACPI_STATUS_INVALID_ARGUMENT;
    };

    return UACPI_STATUS_OK;
}

/*
 * NOTE:
 * 'byte_width' is ALWAYS one of 1, 2, 4. You are NOT allowed to break e.g. a
 * 4-byte access into four 1-byte accesses. Hardware ALWAYS expects accesses to
 * be of the exact width.
 */
uacpi_status uacpi_kernel_raw_io_read(uacpi_io_addr port, uacpi_u8 width, uacpi_u64 *data)
{
    switch (width)
    {
        case 1: *data = port_inb(port); break;
        case 2: *data = port_inw(port); break;
        case 4: *data = port_inl(port); break;
        default: return UACPI_STATUS_INVALID_ARGUMENT;
    }

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_raw_io_write(uacpi_io_addr port, uacpi_u8 width, uacpi_u64 data)
{
    switch (width)
    {
        case 1: port_outb(port, data); break;
        case 2: port_outw(port, data); break;
        case 4: port_outl(port, data); break;
        default: return UACPI_STATUS_INVALID_ARGUMENT;
    }

    return UACPI_STATUS_OK;
}

// -------------------------------------------------------------------------

/*
 * NOTE:
 * 'byte_width' is ALWAYS one of 1, 2, 4. Since PCI registers are 32 bits wide
 * this must be able to handle e.g. a 1-byte access by reading at the nearest
 * 4-byte aligned offset below, then masking the value to select the target
 * byte.
 */
uacpi_status uacpi_kernel_pci_read(uacpi_pci_address *, uacpi_size, uacpi_u8, uacpi_u64 *)
{
    std::cout << "uacpi_kernel_pci_read" << std::endl;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write(uacpi_pci_address *, uacpi_size, uacpi_u8, uacpi_u64)
{
    std::cout << "uacpi_kernel_pci_write" << std::endl;
    return UACPI_STATUS_OK;
}

/*
 * Map a SystemIO address at [base, base + len) and return a kernel-implemented
 * handle that can be used for reading and writing the IO range.
 */
uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size, uacpi_handle *out_handle)
{
    *out_handle = reinterpret_cast<uacpi_handle>(base);
    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle)
{
}

/*
 * Read/Write the IO range mapped via uacpi_kernel_io_map
 * at a 0-based 'offset' within the range.
 *
 * NOTE:
 * 'byte_width' is ALWAYS one of 1, 2, 4. You are NOT allowed to break e.g. a
 * 4-byte access into four 1-byte accesses. Hardware ALWAYS expects accesses to
 * be of the exact width.
 */
uacpi_status uacpi_kernel_io_read(uacpi_handle handle, uacpi_size offset, uacpi_u8 byte_width, uacpi_u64 *value)
{
    auto addr = reinterpret_cast<uacpi_io_addr>(handle);
    return uacpi_kernel_raw_io_read(addr + offset, byte_width, value);
}

uacpi_status uacpi_kernel_io_write(uacpi_handle handle, uacpi_size offset, uacpi_u8 byte_width, uacpi_u64 value)
{
    auto addr = reinterpret_cast<uacpi_io_addr>(handle);
    return uacpi_kernel_raw_io_write(addr + offset, byte_width, value);
}

void *uacpi_kernel_map(uacpi_phys_addr paddr, uacpi_size size)
{
    off_t page_offset = paddr % MOS_PAGE_SIZE;
    const auto npages = ALIGN_UP_TO_PAGE(size) / MOS_PAGE_SIZE;

    if (paddr == MOS_FOURCC('R', 'S', 'D', 'P'))
    {
        fd_t rsdp_fd = open("/sys/acpi/RSDP", O_RDONLY);
        if (rsdp_fd < 0)
            return nullptr;

        void *ptr = mmap(nullptr, npages * MOS_PAGE_SIZE, PROT_READ, MAP_SHARED, rsdp_fd, 0);
        close(rsdp_fd);
        return ptr;
    }

    paddr = ALIGN_DOWN_TO_PAGE(paddr);
    void *ptr = mmap(nullptr, npages * MOS_PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, paddr);
    if (ptr == MAP_FAILED)
        return nullptr;

    return static_cast<uacpi_u8 *>(ptr) + page_offset;
}

void uacpi_kernel_unmap(void *ptr, uacpi_size size)
{
    munmap(ptr, size);
}

/*
 * Allocate a block of memory of 'size' bytes.
 * The contents of the allocated memory are unspecified.
 */
void *uacpi_kernel_alloc(uacpi_size size)
{
    return malloc(size);
}

/*
 * Allocate a block of memory of 'count' * 'size' bytes.
 * The returned memory block is expected to be zero-filled.
 */
void *uacpi_kernel_calloc(uacpi_size count, uacpi_size size)
{
    return calloc(count, size);
}

/*
 * Free a previously allocated memory block.
 *
 * 'mem' might be a NULL pointer. In this case, the call is assumed to be a
 * no-op.
 *
 * An optionally enabled 'size_hint' parameter contains the size of the original
 * allocation. Note that in some scenarios this incurs additional cost to
 * calculate the object size.
 */
void uacpi_kernel_free(void *mem)
{
    free(mem);
}

void uacpi_kernel_log(uacpi_log_level, const uacpi_char *buf)
{
    write(fileno(stdout), buf, strlen(buf));
}

/*
 * Returns the number of 100 nanosecond ticks elapsed since boot,
 * strictly monotonic.
 */
uacpi_u64 uacpi_kernel_get_ticks(void)
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() / 100;
}

/*
 * Spin for N microseconds.
 */
void uacpi_kernel_stall(uacpi_u8 usec)
{
    usleep(usec);
}

/*
 * Sleep for N milliseconds.
 */
void uacpi_kernel_sleep(uacpi_u64 msec)
{
    usleep(msec * 1000);
}

/*
 * Create/free an opaque non-recursive kernel mutex object.
 */
uacpi_handle uacpi_kernel_create_mutex(void)
{
    std::mutex *mutex = new std::mutex;
    return reinterpret_cast<uacpi_handle>(mutex);
}
void uacpi_kernel_free_mutex(uacpi_handle handle)
{
    delete reinterpret_cast<std::mutex *>(handle);
}

/*
 * Create/free an opaque kernel (semaphore-like) event object.
 */

uacpi_handle uacpi_kernel_create_event()
{
    return new char;
}

void uacpi_kernel_free_event(uacpi_handle handle)
{
    delete reinterpret_cast<char *>(handle);
}

/*
 * Returns a unique identifier of the currently executing thread.
 *
 * The returned thread id cannot be UACPI_THREAD_ID_NONE.
 */
uacpi_thread_id uacpi_kernel_get_thread_id(void)
{
    return pthread_self();
}

/*
 * Try to acquire the mutex with a millisecond timeout.
 * A timeout value of 0xFFFF implies infinite wait.
 */
uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle handle, uacpi_u16 timeout)
{
    auto mutex = reinterpret_cast<std::mutex *>(handle);
    if (timeout == 0xFFFF)
    {
        mutex->lock();
        return UACPI_STATUS_OK;
    }

    auto start = std::chrono::steady_clock::now();
    auto end = start + std::chrono::milliseconds(timeout);
    while (!mutex->try_lock())
    {
        if (std::chrono::steady_clock::now() > end)
            return UACPI_STATUS_OK;
    }

    return UACPI_STATUS_OK;
}

void uacpi_kernel_release_mutex(uacpi_handle handle)
{
    reinterpret_cast<std::mutex *>(handle)->unlock();
}

/*
 * Try to wait for an event (counter > 0) with a millisecond timeout.
 * A timeout value of 0xFFFF implies infinite wait.
 *
 * The internal counter is decremented by 1 if wait was successful.
 *
 * A successful wait is indicated by returning UACPI_TRUE.
 */
uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle, uacpi_u16)
{
    return UACPI_FALSE;
}

/*
 * Signal the event object by incrementing its internal counter by 1.
 *
 * This function may be used in interrupt contexts.
 */
void uacpi_kernel_signal_event(uacpi_handle)
{
}

/*
 * Reset the event counter to 0.
 */
void uacpi_kernel_reset_event(uacpi_handle)
{
}

/*
 * Handle a firmware request.
 *
 * Currently either a Breakpoint or Fatal operators.
 */
uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request *)
{
    return UACPI_STATUS_OK;
}

/*
 * Install an interrupt handler at 'irq', 'ctx' is passed to the provided
 * handler for every invocation.
 *
 * 'out_irq_handle' is set to a kernel-implemented value that can be used to
 * refer to this handler from other API.
 */
uacpi_status uacpi_kernel_install_interrupt_handler(uacpi_u32, uacpi_interrupt_handler, uacpi_handle, uacpi_handle *)
{
    return UACPI_STATUS_OK;
}

/*
 * Uninstall an interrupt handler. 'irq_handle' is the value returned via
 * 'out_irq_handle' during installation.
 */
uacpi_status uacpi_kernel_uninstall_interrupt_handler(uacpi_interrupt_handler, uacpi_handle)
{
    return UACPI_STATUS_OK;
}

/*
 * Create/free a kernel spinlock object.
 *
 * Unlike other types of locks, spinlocks may be used in interrupt contexts.
 */
uacpi_handle uacpi_kernel_create_spinlock(void)
{
    spinlock_t *lock = new spinlock_t;
    spinlock_init(lock);
    return reinterpret_cast<uacpi_handle>(lock);
}

void uacpi_kernel_free_spinlock(uacpi_handle handle)
{
    delete reinterpret_cast<spinlock_t *>(handle);
}

/*
 * Lock/unlock helpers for spinlocks.
 *
 * These are expected to disable interrupts, returning the previous state of cpu
 * flags, that can be used to possibly re-enable interrupts if they were enabled
 * before.
 *
 * Note that lock is infalliable.
 */
uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle handle)
{
    uacpi_cpu_flags flags = 0;
    spinlock_acquire(reinterpret_cast<spinlock_t *>(handle));
    return flags;
}

void uacpi_kernel_unlock_spinlock(uacpi_handle handle, uacpi_cpu_flags flags)
{
    MOS_UNUSED(flags);
    spinlock_release(reinterpret_cast<spinlock_t *>(handle));
}

/*
 * Schedules deferred work for execution.
 * Might be invoked from an interrupt context.
 */
std::vector<std::thread> work_threads;
uacpi_status uacpi_kernel_schedule_work(uacpi_work_type type, uacpi_work_handler handler, uacpi_handle ctx)
{
    MOS_UNUSED(type);

    std::thread t([handler, ctx] { handler(ctx); });
    work_threads.push_back(std::move(t));
    return UACPI_STATUS_OK;
}

/*
 * Blocks until all scheduled work is complete and the work queue becomes empty.
 */
uacpi_status uacpi_kernel_wait_for_work_completion(void)
{
    for (auto &t : work_threads)
        t.join();
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address address, uacpi_handle *out_handle)
{
    return UACPI_STATUS_OK;
}

void uacpi_kernel_pci_device_close(uacpi_handle)
{
}

uacpi_status uacpi_kernel_pci_read8(uacpi_handle, uacpi_size, uacpi_u8 *)
{
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read16(uacpi_handle, uacpi_size, uacpi_u16 *)
{
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read32(uacpi_handle, uacpi_size, uacpi_u32 *)
{
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(uacpi_handle, uacpi_size, uacpi_u8)
{
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write16(uacpi_handle, uacpi_size, uacpi_u16)
{
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write32(uacpi_handle, uacpi_size, uacpi_u32)
{
    return UACPI_STATUS_OK;
}
