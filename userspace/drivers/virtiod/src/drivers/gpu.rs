use std::{io::Error, thread::sleep};

use virtio_drivers::{device::gpu::VirtIOGpu, transport::Transport};

use crate::hal::MOSHal;

pub fn run_gpu<T: Transport>(transport: T) -> Result<(), Error> {
    let mut gpu = VirtIOGpu::<MOSHal, T>::new(transport).expect("failed to create gpu driver");
    let (width, height) = gpu.resolution().expect("failed to get resolution");
    let width = width as usize;
    let height = height as usize;
    println!("GPU resolution is {}x{}", width, height);

    let fb = gpu.setup_framebuffer().expect("failed to get fb").as_ptr() as *mut u8;

    let mut n = 0;

    loop {
        for y in 0..height {
            for x in 0..width {
                let idx = ((y * width + x) * 4) as usize;

                let b = fb.wrapping_add(idx);
                let g = fb.wrapping_add(idx + 1);
                let r = fb.wrapping_add(idx + 2);

                unsafe {
                    *b = (n + x + y) as u8;
                    *g = (n + x + y) as u8;
                    *r = (n + x + y) as u8;
                }
            }
        }

        n = n.wrapping_add(50);
        gpu.flush().expect("failed to flush");
        sleep(std::time::Duration::from_millis(100));
    }
}
