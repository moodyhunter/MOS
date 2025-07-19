// SPDX-License-Identifier: GPL-3.0-or-later

use std::thread::sleep;

use librpc_rs::RpcResult;
use virtio_drivers::{device::gpu::VirtIOGpu, transport::Transport};

use crate::hal::MOSHal;

pub fn run_gpu<T: Transport>(transport: T) -> RpcResult<()> {
    let mut gpu = VirtIOGpu::<MOSHal, T>::new(transport).expect("failed to create gpu driver");
    let (width, height) = gpu.resolution().expect("failed to get resolution");
    let width = width as usize;
    let height = height as usize;
    println!("GPU resolution is {}x{}", width, height);

    libsm_rs::report_service_status(libsm_rs::RpcUnitStatusEnum::Started, "gpu-online");

    let fb = gpu.setup_framebuffer().expect("failed to get fb").as_ptr() as *mut u8;

    let cursor_image: Vec<u8> = vec![128; 64 * 64 * 4]; // Assuming a 16x16 cursor with 4 bytes per pixel (RGBA)

    gpu.setup_cursor(cursor_image.as_slice(), 64, 64, 64, 64)
        .expect("failed to setup cursor image");

    let mut n = 600;
    loop {
        for y in 0..height {
            for x in 0..width {
                let idx = ((y * width + x) * 4) as usize;

                let b = fb.wrapping_add(idx + 0);
                let g = fb.wrapping_add(idx + 1);
                let r = fb.wrapping_add(idx + 2);

                unsafe {
                    *b = (((n + x + 0) % (n + 1) + 250 - (n % 250)) % 255) as u8;
                    *g = (((n + 0 + y) % (n + 1) + 250 - (n % 250)) % 255) as u8;
                    *r = (((n + x + y) % (n + 1) + 250 - (n % 250)) % 255) as u8;
                }
            }
        }

        n = n.wrapping_add(2);
        gpu.flush().expect("failed to flush");
        sleep(std::time::Duration::from_millis(1000 / 60)); // 60 FPS
    }
}
