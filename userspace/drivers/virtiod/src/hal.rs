// SPDX-License-Identifier: GPL-3.0-or-later

use virtio_drivers::{Hal, PAGE_SIZE};

use core::ptr::NonNull;

extern "C" {

    // defined in libdma.c

    // void libdma_init()
    pub fn libdma_init();

    // bool libdma_alloc(u64 size, ptr_t *phys, ptr_t *virt)
    pub fn libdma_alloc(n_pages: usize, phys: *mut usize, virt: *mut *mut u8) -> bool;

    // bool libdma_dealloc(ptr_t virt, ptr_t phys, size_t n_pages)
    pub fn libdma_dealloc(virt: *const u8, phys: usize, n_pages: usize) -> bool;

    // bool libdma_share_buffer(void *buffer, size_t size, ptr_t *phyaddr)
    pub fn libdma_share_buffer(buffer: *mut u8, size: usize, phyaddr: *mut usize) -> bool;

    // bool libdma_unshare_buffer(ptr_t phyaddr, void *buffer, size_t size)
    pub fn libdma_unshare_buffer(phyaddr: usize, buffer: *mut u8, size: usize) -> bool;

    // ptr_t libdma_map_physical_address(ptr_t paddr, size_t size, ptr_t vaddr)
    pub fn libdma_map_physical_address(paddr: usize, size: usize, vaddr: *mut u8) -> usize;

    // void libdma_exit(void)
    pub fn libdma_exit();

}

pub(crate) struct MOSHal {}

macro_rules! align_up {
    ($value:expr, $alignment:expr) => {
        ($value + ($alignment - 1)) & !($alignment - 1)
    };
}

unsafe impl Hal for MOSHal {
    fn dma_alloc(
        pages: usize,
        _direction: virtio_drivers::BufferDirection,
    ) -> (virtio_drivers::PhysAddr, NonNull<u8>) {
        let mut paddr: usize = 0;
        let mut vaddr: *mut u8 = std::ptr::null_mut();

        if unsafe { !libdma_alloc(pages, &mut paddr, &mut vaddr) } {
            panic!("Failed to allocate DMA buffer");
        }

        (paddr, NonNull::new(vaddr).unwrap())
    }

    unsafe fn dma_dealloc(
        paddr: virtio_drivers::PhysAddr,
        vaddr: NonNull<u8>,
        n_pages: usize,
    ) -> i32 {
        let result = libdma_dealloc(vaddr.as_ptr(), paddr as usize, n_pages);
        !result as i32 // true -> 0, false -> 1
    }

    unsafe fn mmio_phys_to_virt(paddr: virtio_drivers::PhysAddr, size: usize) -> NonNull<u8> {
        let npages = align_up!(size, PAGE_SIZE) / PAGE_SIZE;
        let vaddr = libdma_map_physical_address(paddr as usize, npages, paddr as _);
        if vaddr == 0 {
            panic!("Failed to map physical address");
        }
        NonNull::new(vaddr as *mut u8).unwrap()
    }

    unsafe fn share(
        buffer: NonNull<[u8]>,
        _direction: virtio_drivers::BufferDirection,
    ) -> virtio_drivers::PhysAddr {
        let mut phyaddr: usize = 0;
        if unsafe { !libdma_share_buffer(buffer.as_ptr() as *mut u8, buffer.len(), &mut phyaddr) } {
            panic!("Failed to share buffer");
        }
        phyaddr
    }

    unsafe fn unshare(
        paddr: virtio_drivers::PhysAddr,
        buffer: NonNull<[u8]>,
        _direction: virtio_drivers::BufferDirection,
    ) {
        if !libdma_unshare_buffer(paddr as usize, buffer.as_ptr() as *mut u8, buffer.len()) {
            panic!("Failed to unshare buffer");
        }
    }
}
