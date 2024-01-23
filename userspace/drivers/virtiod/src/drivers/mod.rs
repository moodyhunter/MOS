use std::io::Error;

use virtio_drivers::transport::{
    pci::{bus::DeviceFunction, PciTransport},
    DeviceType, Transport,
};

use self::{block::run_blockdev, gpu::run_gpu};

mod block;
mod gpu;

pub(crate) fn start_device(transport: PciTransport, function: DeviceFunction) -> Result<(), Error> {
    match transport.device_type() {
        DeviceType::Block => run_blockdev(transport, function),
        DeviceType::GPU => run_gpu(transport),
        t => unimplemented!("Unrecognized virtio device: {:?}", t),
    }
}
