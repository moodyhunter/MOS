// SPDX-License-Identifier: GPL-3.0-or-later

mod block;
mod gpu;
mod netdev;

use os::unix::thread;
use std::{error::Error, ffi::CString, os};
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

    // set thread name to device type
    let thread_name = match device_type {
        DeviceType::Block => "virtiod-block",
        DeviceType::GPU => "virtiod-gpu",
        DeviceType::Network => "virtiod-net",
        _ => "virtiod-unknown",
    };

    {
        let cstring = CString::new(thread_name).unwrap();
        unsafe { libc::pthread_setname_np(libc::pthread_self(), cstring.as_ptr()) };
    }

    match device_type {
        DeviceType::Block => run_blockdev(transport, function),
        DeviceType::GPU => run_gpu(transport, function),
        DeviceType::Network => run_netdev(transport, function),
        t => unimplemented!("Unrecognized virtio device: {:?}", t),
    }
}
