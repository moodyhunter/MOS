// SPDX-License-Identifier: GPL-3.0-or-later

#include <bits/null.h>
#include <fcntl.h>
#include <mos/mm/mm_types.h>
#include <mos/syscall/usermode.h>
#include <mos/types.h>
#include <sys/mman.h>
#include <unistd.h>

// #define LIBDMA_DEBUG

#if defined(LIBDMA_DEBUG)
#include <stdio.h>
#define libdma_debug(fmt, ...) printf("libdma: " fmt "\n"__VA_OPT__(, ) __VA_ARGS__)
#else
#define libdma_debug(...)
#endif

static fd_t sysmem_fd = -1;

void libdma_init(void)
{
    sysmem_fd = open("/sys/mem", O_RDWR);
    libdma_debug("libdma is initialized, /sys/mem fd=%d", sysmem_fd);
}

bool libdma_alloc(size_t n_pages, ptr_t *phys, ptr_t *virt)
{
    return syscall_dmabuf_alloc(n_pages, phys, virt);
}

bool libdma_dealloc(ptr_t virt, ptr_t phys, size_t n_pages)
{
    MOS_UNUSED(n_pages);
    return syscall_dmabuf_free(virt, phys);
}

bool libdma_share_buffer(void *buffer, size_t size, ptr_t *phyaddr)
{
    bool result = syscall_dmabuf_share(buffer, size, phyaddr);
    libdma_debug("share buffer: phyaddr=" PTR_FMT ", buffer=%p, size=%zu", *phyaddr, buffer, size);
    return result;
}

bool libdma_unshare_buffer(ptr_t phyaddr, void *buffer, size_t size)
{
    bool result = syscall_dmabuf_unshare(phyaddr, size, buffer);
    libdma_debug("unshare buffer: phyaddr=" PTR_FMT ", buffer=%p, size=%zu", phyaddr, buffer, size);
    return result;
}

ptr_t libdma_map_physical_address(ptr_t paddr, size_t n_pages, ptr_t vaddr)
{
    paddr = ALIGN_DOWN_TO_PAGE(paddr);
    mmap_flags_t flags = MMAP_SHARED;
    if (vaddr)
        flags |= MMAP_EXACT;
    return (ptr_t) syscall_mmap_file(vaddr, n_pages * MOS_PAGE_SIZE, MEM_PERM_READ | MEM_PERM_WRITE, flags, sysmem_fd, paddr);
}

void libdma_exit(void)
{
    close(sysmem_fd);
    libdma_debug("libdma exits");
}
