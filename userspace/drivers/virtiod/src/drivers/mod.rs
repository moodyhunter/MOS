// SPDX-License-Identifier: GPL-3.0-or-later

mod block;
mod gpu;

use std::error::Error;

use virtio_drivers::transport::{
    pci::{bus::DeviceFunction, PciTransport},
    DeviceType, Transport,
};

use self::{block::run_blockdev, gpu::run_gpu};

pub(crate) fn start_device(
    transport: PciTransport,
    function: DeviceFunction,
) -> Result<(), Box<dyn Error>> {
    match transport.device_type() {
        DeviceType::Block => run_blockdev(transport, function),
        DeviceType::GPU => Ok(run_gpu(transport)?),
        t => unimplemented!("Unrecognized virtio device: {:?}", t),
    }
}
