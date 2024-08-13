// SPDX-License-Identifier: GPL-3.0-or-later

use std::sync::{Arc, Mutex};

use librpc_rs::{
    rpc_server_function, RpcCallContext, RpcCallFuncInfo, RpcResult, RpcServer, RpcStub,
};
use protobuf::MessageField;
use virtio_drivers::{
    device::blk::{VirtIOBlk, SECTOR_SIZE},
    transport::pci::{bus::DeviceFunction, PciTransport},
};

use crate::{
    hal::MOSHal,
    mosrpc::{
        self,
        blockdev::{
            Read_block_request, Read_block_response, Register_device_request,
            Register_device_response, Write_block_request, Write_block_response,
        },
    },
    result_err, result_ok,
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
    pub fn register(&mut self) -> RpcResult<()> {
        let dev = &self.blockdev.lock().unwrap().0;

        let request = Register_device_request {
            server_name: self.server_name.clone(),
            device_info: Some(mosrpc::blockdev::Blockdev_info {
                name: self.devname.clone(),
                block_size: SECTOR_SIZE as _, // 512
                n_blocks: dev.capacity(),
                size: dev.capacity(),
                ..Default::default()
            })
            .into(),
            ..Default::default()
        };

        let resp: Register_device_response = self.blockdev_manager.create_pb_call(1, &request)?;

        if !resp.result.success {
            return Err(Box::new(std::io::Error::new(
                std::io::ErrorKind::AddrInUse,
                resp.result
                    .error
                    .clone()
                    .unwrap_or("unknown error".to_string()),
            )));
        }

        Ok(())
    }

    pub fn on_read(&mut self, ctx: &mut RpcCallContext) -> RpcResult<()> {
        let arg: Read_block_request = ctx.get_arg_pb(0)?;
        let mut buf = vec![0u8; 512 * arg.n_blocks as usize];

        let virtioblk = &mut self.blockdev.lock().unwrap().0;

        let resp = match virtioblk.read_blocks(arg.n_boffset as _, &mut buf) {
            Ok(()) => Read_block_response {
                result: result_ok!(),
                data: buf,
                ..Default::default()
            },
            Err(e) => Read_block_response {
                result: result_err!(format!("failed to read: {}", e)),
                ..Default::default()
            },
        };

        ctx.write_response_pb(&resp)
    }

    pub fn on_write(&mut self, ctx: &mut RpcCallContext) -> RpcResult<()> {
        let arg: Write_block_request = ctx.get_arg_pb(0)?;
        assert!(arg.data.len() == 512 * arg.n_blocks as usize);

        let virtioblk = &mut self.blockdev.lock().unwrap().0;

        let resp = match virtioblk.write_blocks(arg.n_boffset as _, &arg.data) {
            Ok(()) => Write_block_response {
                result: result_ok!(),
                ..Default::default()
            },
            Err(e) => Write_block_response {
                result: result_err!(format!("failed to write: {}", e)),
                ..Default::default()
            },
        };

        ctx.write_response_pb(&resp)
    }
}

const FUNCTIONS: &[RpcCallFuncInfo<BlockDevDriver>] = &[
    rpc_server_function!(1, BlockDevDriver::on_read, Buffer),
    rpc_server_function!(2, BlockDevDriver::on_write, Buffer),
];

pub fn run_blockdev(transport: PciTransport, function: DeviceFunction) -> RpcResult<()> {
    let devlocation = format!(
        "{:02x}:{:02x}:{:02x}",
        function.bus, function.device, function.function
    );

    let devname = format!("virtblk.{}", devlocation);
    let server_name = format!("blockdev.virtio.{}", devlocation);

    // check /dev/block if the device is already registered
    if std::fs::metadata(format!("/dev/block/{}", devname)).is_ok() {
        println!("blockdev {} already registered", devname);
        return Ok(());
    }

    let mut driver = BlockDevDriver {
        blockdev_manager: RpcStub::new("mos.blockdev-manager")?,
        devname,
        server_name,
        blockdev: Arc::new(Mutex::new(SafeVirtIOBlk(
            VirtIOBlk::new(transport).expect("failed to create blockdev"),
        ))),
    };

    let mut rpc_server = RpcServer::create(&driver.server_name, FUNCTIONS)?;

    driver.register()?;

    rpc_server.run(&mut driver)
}
