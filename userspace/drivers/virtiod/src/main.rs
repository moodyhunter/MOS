use crate::{
    drivers::start_device,
    hal::{libdma_init, libdma_map_physical_address},
};
use clap::Parser;
use hal::MOSHal;
use std::println;
use utils::{parse_hex16, parse_hex32, parse_hex64};
use virtio_drivers::transport::{
    pci::{
        bus::{Cam, Command, DeviceFunction, PciRoot},
        PciTransport,
    },
    Transport,
};

mod drivers;
mod hal;
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
    log::set_max_level(log::LevelFilter::Trace);
    let args = Args::parse();
    unsafe { libdma_init() };

    println!("Vendor: 0x{:x}", args.vendor_id);
    println!("Device: 0x{:x}", args.device_id);
    println!("Location: 0x{:x}", args.location);
    println!("MMIO Base: 0x{:x}", args.mmio_base);

    // const u32 location = (bus << 16) | (device << 8) | function;
    let bus = (args.location >> 16) as u8;
    let device = ((args.location >> 8) & 0xff) as u8;
    let function = (args.location & 0xff) as u8;

    println!(
        "bus: {:x}, device: {:x}, function: {:x}",
        bus, device, function
    );

    let mmconfig_base =
        unsafe { libdma_map_physical_address(args.mmio_base as _, 256, std::ptr::null_mut()) };

    let mut pci_root = unsafe { PciRoot::new(mmconfig_base as _, Cam::Ecam) };
    let device_function = DeviceFunction {
        bus,
        device,
        function,
    };

    let (status, command) = pci_root.get_status_command(device_function);
    println!(
        "Found {} at {}, status {:?} command {:?}",
        device_function, device_function, status, command
    );

    // Enable the device to use its BARs.
    pci_root.set_command(
        device_function,
        Command::IO_SPACE | Command::MEMORY_SPACE | Command::BUS_MASTER,
    );

    for bar in 0..6 {
        let bar_info = pci_root.bar_info(device_function, bar).unwrap();
        println!("BAR {}: {:#x?}", bar, bar_info);
    }

    let mut transport = PciTransport::new::<MOSHal>(&mut pci_root, device_function).unwrap();

    println!(
        "Detected virtio PCI device with device type {:?}, features {:#018x}",
        transport.device_type(),
        transport.read_device_features(),
    );

    start_device(transport);
}
