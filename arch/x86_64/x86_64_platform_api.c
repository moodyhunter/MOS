// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/mos_global.h>
#include <mos/platform/platform.h>

static u64 rdtsc(void)
{
    u32 lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((u64) hi << 32) | lo;
}

// Platform Machine APIs
noreturn void platform_shutdown(void)
{
    while (1)
        ;
}

// Platform CPU APIs

noreturn void platform_halt_cpu(void)
{
    __asm__ volatile("hlt");
    while (1)
        ;
}

u32 platform_current_cpu_id(void)
{
    u32 cpu_id;
    __asm__ volatile("mov %%gs:0x0, %0" : "=r"(cpu_id));
    return cpu_id;
}

void platform_msleep(u64 ms)
{
    u64 start = rdtsc();
    u64 end = start + (ms * 1000 * 1000);
    while (rdtsc() < end)
        ;
}

void platform_usleep(u64 us)
{
    u64 start = rdtsc();
    u64 end = start + (us * 1000);
    while (rdtsc() < end)
        ;
}

// Platform Interrupt APIs

void platform_interrupt_enable(void)
{
}

void platform_interrupt_disable(void)
{
}

bool platform_irq_handler_install(u32 irq, irq_handler handler)
{
    MOS_UNUSED(irq);
    MOS_UNUSED(handler);
    return true;
}

void platform_irq_handler_remove(u32 irq, irq_handler handler)
{
    MOS_UNUSED(irq);
    MOS_UNUSED(handler);
}

// Platform Page Table APIs

paging_handle_t platform_mm_create_user_pgd(void)
{
    return (paging_handle_t){ .pgd = 0, .pgd_lock = 0, .um_page_map = 0 };
}

void platform_mm_destroy_user_pgd(mm_context_t *mmctx)
{
    MOS_UNUSED(table);
}

// Platform Paging APIs

void platform_mm_map_pages(mm_context_t *mmctx, ptr_t vaddr, ptr_t paddr, size_t n_pages, vm_flags flags)
{
    MOS_UNUSED(table);
    MOS_UNUSED(vaddr);
    MOS_UNUSED(paddr);
    MOS_UNUSED(n_pages);
    MOS_UNUSED(flags);
}

void platform_mm_unmap_pages(mm_context_t *mmctx, ptr_t vaddr, size_t n_pages)
{
    MOS_UNUSED(table);
    MOS_UNUSED(vaddr);
    MOS_UNUSED(n_pages);
}

ptr_t platform_mm_get_phys_addr(mm_context_t *mmctx, ptr_t vaddr)
{
    MOS_UNUSED(table);
    MOS_UNUSED(vaddr);
    return 0;
}

vmblock_t platform_mm_copy_maps(paging_handle_t from, ptr_t fvaddr, paging_handle_t to, ptr_t tvaddr, size_t npages)
{
    MOS_UNUSED(from);
    MOS_UNUSED(fvaddr);
    MOS_UNUSED(to);
    MOS_UNUSED(tvaddr);
    MOS_UNUSED(npages);
    return (vmblock_t){ 0 };
}

void platform_mm_flag_pages(mm_context_t *mmctx, ptr_t vaddr, size_t n, vm_flags flags)
{
    MOS_UNUSED(table);
    MOS_UNUSED(vaddr);
    MOS_UNUSED(n);
    MOS_UNUSED(flags);
}

vm_flags platform_mm_get_flags(mm_context_t *mmctx, ptr_t vaddr)
{
    MOS_UNUSED(table);
    MOS_UNUSED(vaddr);
    return 0;
}

// Platform Thread / Process APIs

void platform_context_setup(thread_t *thread, thread_entry_t entry, void *arg)
{
    MOS_UNUSED(thread);
    MOS_UNUSED(entry);
    MOS_UNUSED(arg);
}

void platform_setup_forked_context(const thread_context_t *from, thread_context_t **to)
{
    MOS_UNUSED(from);
    MOS_UNUSED(to);
}

// Platform Context Switching APIs

void platform_switch_to_thread(ptr_t *old_stack, const thread_t *new_thread, switch_flags_t switch_flags)
{
    MOS_UNUSED(old_stack);
    MOS_UNUSED(new_thread);
    MOS_UNUSED(switch_flags);
}

void platform_switch_to_scheduler(ptr_t *old_stack, ptr_t new_stack)
{
    MOS_UNUSED(old_stack);
    MOS_UNUSED(new_stack);
}

// Platform-Specific syscall APIs
u64 platform_arch_syscall(u64 syscall, u64 arg1, u64 arg2, u64 arg3, u64 arg4)
{
    MOS_UNUSED(syscall);
    MOS_UNUSED(arg1);
    MOS_UNUSED(arg2);
    MOS_UNUSED(arg3);
    MOS_UNUSED(arg4);
    return -1;
}

void platform_mm_iterate_table(mm_context_t *mmctx, ptr_t vaddr, size_t n, pgt_iteration_callback_t callback, void *arg)
{
    MOS_UNUSED(table);
    MOS_UNUSED(vaddr);
    MOS_UNUSED(n);
    MOS_UNUSED(callback);
    MOS_UNUSED(arg);
}

void platform_invalidate_tlb()
{
}

void platform_ipi_send(u8 target_cpu, ipi_type_t type)
{
    MOS_UNUSED(target_cpu);
    MOS_UNUSED(type);
}
