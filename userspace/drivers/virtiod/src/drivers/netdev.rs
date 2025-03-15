// SPDX-License-Identifier: GPL-3.0-or-later

use librpc_rs::RpcResult;
use virtio_drivers::{
    device::net::VirtIONet,
    transport::pci::{bus::DeviceFunction, PciTransport},
};

use crate::MOSHal;

pub fn run_netdev(transport: PciTransport, _func: DeviceFunction) -> RpcResult<()> {
    let mut netdev = VirtIONet::<MOSHal, PciTransport, 16>::new(transport, 4096).unwrap();

    netdev.enable_interrupts();

    let mac = netdev.mac_address();
    let mac = format!(
        "{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
    );
    println!("MAC address: {}", mac);

    Ok(())
}
