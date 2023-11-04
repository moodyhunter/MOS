use virtio_drivers::transport::{DeviceType, Transport};

use self::{block::start_block_driver, gpu::start_gpu_driver};

mod block;
mod gpu;

pub(crate) fn start_device(transport: impl Transport) {
    match transport.device_type() {
        DeviceType::Block => start_block_driver(transport),
        DeviceType::GPU => start_gpu_driver(transport),
        t => println!("Unrecognized virtio device: {:?}", t),
    }
}
