use std::{
    cell::OnceCell,
    io::Error,
    sync::{Arc, Mutex},
};

use librpc_rs::{rpc_server_function, RpcCallContext, RpcServer, RpcStub};
use virtio_drivers::{
    device::blk::VirtIOBlk,
    transport::pci::{bus::DeviceFunction, PciTransport},
};

use crate::{
    hal::MOSHal,
    mos_rpc::blockdev::{Read_request, Register_dev_request, Register_dev_response, Write_request},
};

#[derive(Clone)]
struct BlockDevDriver {
    blockdev_manager: RpcStub,
    devname: String,
}

static mut DEVICE: OnceCell<Arc<Mutex<VirtIOBlk<MOSHal, PciTransport>>>> = OnceCell::new();

const VIRTIO_BLOCKDEV_SERVER_NAME: &str = "virtio-blockdev";

impl BlockDevDriver {
    pub fn register(&mut self) -> Result<(), Error> {
        let dev = unsafe { DEVICE.get() }.unwrap().lock().unwrap();

        let mut req = Register_dev_request::default();
        req.block_size = 512;
        req.num_blocks = dev.capacity() / 512;
        req.server_name = VIRTIO_BLOCKDEV_SERVER_NAME.to_string();
        req.blockdev_name = self.devname.to_string();

        println!("req: {:?}", req);
        let resp: Register_dev_response = self.blockdev_manager.create_pb_call(1, &req)?;
        println!("resp: {:?}", resp);
        Ok(())
    }

    pub fn on_read(&mut self, ctx: &mut RpcCallContext) -> Result<(), Error> {
        let arg: Read_request = ctx.get_arg_pb(0)?;
        println!("arg: {:?}", arg);

        Ok(())
    }

    pub fn on_write(&mut self, ctx: &mut RpcCallContext) -> Result<(), Error> {
        let arg: Write_request = ctx.get_arg_pb(0)?;
        println!("arg: {:?}", arg);
        Ok(())
    }

    // let mut input = [0xffu8; 512];
    // let mut output = [0; 512];
    // for i in 0..32 {
    //     let mut j = 0;
    //     for x in input.iter_mut() {
    //         *x = j as u8;
    //         j += 1;
    //     }

    //     blk.write_blocks(i, &input).expect("failed to write");
    //     blk.read_blocks(i, &mut output).expect("failed to read");
    //     assert_eq!(input, output);
    //     println!("read/write block {} passed", i);
    // }
}

// virtiod --vendor-id 1af4 --device-id 1001 --location 0300 --mmio-base b0000000
pub fn run_blockdev(transport: PciTransport, function: DeviceFunction) -> Result<(), Error> {
    unsafe {
        assert!(DEVICE.get().is_none());
        match DEVICE.set(Arc::new(Mutex::new(
            VirtIOBlk::new(transport).expect("failed to create blockdev"),
        ))) {
            Ok(_) => {}
            Err(_) => panic!("failed to set DEVICE"),
        }
    };

    let mut driver = BlockDevDriver {
        blockdev_manager: RpcStub::new("mos.blockdev-manager")?,
        devname: format!(
            "virtio-blockdev-{:02x}:{:02x}:{:02x}",
            function.bus, function.device, function.function
        ),
    };

    let functions = &[
        rpc_server_function!(0, BlockDevDriver::on_read, Buffer),
        rpc_server_function!(1, BlockDevDriver::on_write, Buffer),
    ];

    let mut rpc_server = RpcServer::create(VIRTIO_BLOCKDEV_SERVER_NAME, functions)?;

    driver.register()?;

    println!("rpc server running...");
    rpc_server.run(&mut driver)
}
