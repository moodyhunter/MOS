// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    drivers::start_device,
    hal::{libdma_exit, libdma_init, libdma_map_physical_address},
};
use clap::Parser;
use hal::MOSHal;
use utils::parse_hex64;
use virtio_drivers::transport::pci::{
    bus::{Cam, Command, DeviceFunction, MmioCam, PciRoot},
    PciTransport,
};

mod drivers;
mod hal;
mod mosrpc;
mod utils;

/// VirtIO Driver for MOS
#[derive(Parser, Debug)]
#[command(author, version, about, long_about = None)]
struct Args {
    /// PCIe Device Location - Bus
    #[arg(long)]
    bus: u32,

    /// PCIe Device Location - Device
    #[arg(long)]
    dev: u32,

    /// PCIe Device Location - Function
    #[arg(long)]
    func: u32,

    /// MMIO Base Address
    #[arg(short, long, value_parser=parse_hex64, default_value_t = 0xe0000000)]
    mmio_base: u64,
}

fn main() -> () {
    println!("VirtIO Driver for MOS");

    let args = Args::parse();

    let location = DeviceFunction {
        bus: args.bus as u8,
        device: args.dev as u8,
        function: args.func as u8,
    };

    #[cfg(feature = "debug")]
    println!(
        "Device Location: 0x{:x}, Device: 0x{:x}, Function: 0x{:x}",
        location.bus, location.device, location.function
    );

    let mut pci_root = unsafe {
        libdma_init();
        PciRoot::new(MmioCam::new(
            libdma_map_physical_address(args.mmio_base as _, 256, std::ptr::null_mut()) as _,
            Cam::Ecam,
        ))
    };

    // Enable the device to use its BAR.
    pci_root.set_command(
        location,
        Command::IO_SPACE | Command::MEMORY_SPACE | Command::BUS_MASTER,
    );

    let transport = PciTransport::new::<MOSHal, MmioCam>(&mut pci_root, location).unwrap();

    start_device(transport, location).expect("Failed to start device");

    unsafe {
        libdma_exit();
    }
}
