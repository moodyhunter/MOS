// SPDX-License-Identifier: GPL-3.0-or-later

mod block;
mod gpu;
mod netdev;

use std::error::Error;

use virtio_drivers::transport::{
    pci::{bus::DeviceFunction, PciTransport},
    DeviceType, Transport,
};

use self::{block::run_blockdev, gpu::run_gpu, netdev::run_netdev};

pub(crate) fn start_device(
    transport: PciTransport,
    function: DeviceFunction,
) -> Result<(), Box<dyn Error>> {
    let device_type = transport.device_type();
    println!("  Device Type: {:?}", device_type);
    println!("  Device Function: {:?}", function);

    match device_type {
        DeviceType::Block => run_blockdev(transport, function),
        DeviceType::GPU => run_gpu(transport),
        DeviceType::Network => run_netdev(transport, function),
        t => unimplemented!("Unrecognized virtio device: {:?}", t),
    }
}
