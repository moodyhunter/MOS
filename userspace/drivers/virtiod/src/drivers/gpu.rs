use std::thread::sleep;

use virtio_drivers::{device::gpu::VirtIOGpu, transport::Transport};

use crate::hal::MOSHal;

pub fn start_gpu_driver<T: Transport>(transport: T) {
    let mut gpu = VirtIOGpu::<MOSHal, T>::new(transport).expect("failed to create gpu driver");
    let (width, height) = gpu.resolution().expect("failed to get resolution");
    let width = width as usize;
    let height = height as usize;
    println!("GPU resolution is {}x{}", width, height);

    let fb = gpu.setup_framebuffer().expect("failed to get fb");

    for y in 0..height {
        for x in 0..width {
            let idx = (y * width + x) * 4;
            fb[idx] = x as u8;
            fb[idx + 1] = y as u8;
            fb[idx + 2] = (x + y) as u8;
        }
    }

    gpu.flush().expect("failed to flush");

    println!("virtio-gpu show graphics...");
    sleep(std::time::Duration::from_secs(10));
    println!("virtio-gpu test finished");
}
