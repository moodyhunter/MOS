use crate::{
    drivers::start_device,
    hal::{libdma_init, libdma_map_physical_address},
};
use clap::Parser;
use hal::MOSHal;
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
mod mos_rpc;
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
    println!("  Device Info");
    println!("    Vendor: 0x{:x}", args.vendor_id);
    println!("    Device: 0x{:x}", args.device_id);
    println!("    MMIO Base: 0x{:x}", args.mmio_base);

    // const u32 location = (bus << 16) | (device << 8) | function;
    let bus = (args.location >> 16) as u8;
    let device = ((args.location >> 8) & 0xff) as u8;
    let function = (args.location & 0xff) as u8;

    println!("  PCI Info");
    println!("    Bus: 0x{:x}", bus);
    println!("    Device: 0x{:x}", device);
    println!("    Function: 0x{:x}", function);

    let mut pci_root = unsafe {
        libdma_init();
        PciRoot::new(
            libdma_map_physical_address(args.mmio_base as _, 256, std::ptr::null_mut()) as _,
            Cam::Ecam,
        )
    };

    let device_function = DeviceFunction {
        bus,
        device,
        function,
    };

    let (status, command) = pci_root.get_status_command(device_function);
    println!(
        "Found {}, status {:?} command {:?}",
        device_function, status, command
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
        "Detected virtio PCI device '{:?}', feature={:#018x}",
        transport.device_type(),
        transport.read_device_features(),
    );

    start_device(transport, device_function).expect("failed to start device")
}
