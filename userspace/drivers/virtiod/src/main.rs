// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    drivers::start_device,
    hal::{libdma_exit, libdma_init, libdma_map_physical_address},
};
use clap::Parser;
use hal::MOSHal;
use utils::{parse_hex16, parse_hex32, parse_hex64};
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
    /// PCI Vendor ID
    #[arg(long, value_parser=parse_hex16, default_value_t = 0x1af4)]
    vendor_id: u16,

    /// PCIe Device ID
    #[arg(long, value_parser=parse_hex16)]
    device_id: u16,

    /// PCIe Device Location
    #[arg(long, value_parser=parse_hex32)]
    location: u32,

    /// MMIO Base Address
    #[arg(short, long, value_parser=parse_hex64, default_value_t = 0xb0000000)]
    mmio_base: u64,
}

fn main() -> () {
    println!("VirtIO Driver for MOS");

    let args = Args::parse();

    #[cfg(feature = "debug")]
    {
        println!("  Device Info");
        println!("    Vendor: 0x{:x}", args.vendor_id);
        println!("    Device: 0x{:x}", args.device_id);
        println!("    MMIO Base: 0x{:x}", args.mmio_base);
    }

    let location = DeviceFunction {
        bus: (args.location >> 16) as u8,
        device: ((args.location >> 8) & 0xff) as u8,
        function: (args.location & 0xff) as u8,
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

    #[cfg(feature = "debug")]
    {
        let (status, command) = pci_root.get_status_command(device_function);
        println!(
            "Found {}, status {:?} command {:?}",
            device_function, status, command
        );
    }

    // Enable the device to use its BAR.
    pci_root.set_command(
        location,
        Command::IO_SPACE | Command::MEMORY_SPACE | Command::BUS_MASTER,
    );

    #[cfg(feature = "debug")]
    for bar in 0..6 {
        let bar_info = pci_root.bar_info(device_function, bar).unwrap();
        println!("BAR {}: {:#x?}", bar, bar_info);
    }

    let transport = PciTransport::new::<MOSHal, MmioCam>(&mut pci_root, location).unwrap();

    start_device(transport, location).expect("Failed to start device");

    unsafe {
        libdma_exit();
    }
}
