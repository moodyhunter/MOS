use std::{
    io::Error,
    sync::{Arc, Mutex},
};

use librpc_rs::{rpc_server_function, RpcCallContext, RpcCallFuncInfo, RpcServer, RpcStub};
use protobuf::MessageField;
use virtio_drivers::{
    device::blk::VirtIOBlk,
    transport::pci::{bus::DeviceFunction, PciTransport},
};

use crate::{
    hal::MOSHal,
    mos_rpc::blockdev::{
        Read_request, Read_response, Register_dev_request, Register_dev_response, Write_request,
        Write_response,
    },
    result_ok,
};

struct SafeVirtIOBlk(VirtIOBlk<MOSHal, PciTransport>);
unsafe impl Send for SafeVirtIOBlk {}

#[derive(Clone)]
struct BlockDevDriver {
    blockdev_manager: RpcStub,
    devname: String,
    server_name: String,
    blockdev: Arc<Mutex<SafeVirtIOBlk>>,
}

impl BlockDevDriver {
    pub fn register(&mut self) -> Result<(), Error> {
        let dev = &self.blockdev.lock().unwrap().0;

        let request = Register_dev_request {
            block_size: 512,
            num_blocks: dev.capacity() / 512,
            server_name: self.server_name.clone(),
            blockdev_name: self.devname.clone(),
            ..Default::default()
        };

        let resp: Register_dev_response = self.blockdev_manager.create_pb_call(1, &request)?;
        println!("registered with blockdev id {}", resp.id);
        Ok(())
    }

    pub fn on_read(&mut self, ctx: &mut RpcCallContext) -> Result<(), Error> {
        let dev = &mut self.blockdev.lock().unwrap().0;

        let arg: Read_request = ctx.get_arg_pb(0)?;
        println!(
            "read reqeust: block {}, nblocks {}",
            arg.n_boffset, arg.n_blocks
        );

        let mut buf = vec![0u8; 512 * arg.n_blocks as usize];

        dev.read_blocks(arg.n_boffset as _, &mut buf)
            .expect("failed to read");

        let resp = Read_response {
            result: result_ok!(),
            data: buf,
            ..Default::default()
        };

        ctx.write_response_pb(&resp)
    }

    pub fn on_write(&mut self, ctx: &mut RpcCallContext) -> Result<(), Error> {
        let arg: Write_request = ctx.get_arg_pb(0)?;

        println!(
            "write reqeust: block {}, nblocks {}",
            arg.n_boffset, arg.n_blocks
        );

        // let mut dev = unsafe { BLOCKDEV.get() }.unwrap().lock().unwrap();
        let dev = &mut self.blockdev.lock().unwrap().0;

        dev.write_blocks(arg.n_boffset as _, &arg.data)
            .expect("failed to write");

        let resp = Write_response {
            result: result_ok!(),
            ..Default::default()
        };

        ctx.write_response_pb(&resp)
    }
}

const FUNCTIONS: &[RpcCallFuncInfo<BlockDevDriver>] = &[
    rpc_server_function!(1, BlockDevDriver::on_read, Buffer),
    rpc_server_function!(2, BlockDevDriver::on_write, Buffer),
];

// virtiod --vendor-id 1af4 --device-id 1001 --location 0300 --mmio-base b0000000
pub fn run_blockdev(transport: PciTransport, function: DeviceFunction) -> Result<(), Error> {
    let devname = format!(
        "{:02x}:{:02x}:{:02x}",
        function.bus, function.device, function.function
    );

    let mut driver = BlockDevDriver {
        blockdev_manager: RpcStub::new("mos.blockdev-manager")?,
        devname: format!("virtio-blockdev:{}", devname),
        server_name: format!("blockdev.virtio.{}", devname),
        blockdev: Arc::new(Mutex::new(SafeVirtIOBlk(
            VirtIOBlk::new(transport).expect("failed to create blockdev"),
        ))),
    };

    let mut rpc_server = RpcServer::create(&driver.server_name, FUNCTIONS)?;

    driver.register()?;

    println!("rpc server running...");
    rpc_server.run(&mut driver)
}
