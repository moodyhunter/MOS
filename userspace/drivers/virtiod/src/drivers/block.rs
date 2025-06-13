// SPDX-License-Identifier: GPL-3.0-or-later

use std::sync::{Arc, Mutex};

use librpc_rs::{RpcCallResult, RpcPbReplyEnum, RpcPbServer, RpcPbServerTrait, RpcResult, RpcStub};
use protobuf::MessageField;
use virtio_drivers::{
    device::blk::{VirtIOBlk, SECTOR_SIZE},
    transport::pci::{bus::DeviceFunction, PciTransport},
};

use crate::{
    hal::MOSHal,
    mosrpc::blockdev::{
        Blockdev_info, Read_block_request, Read_block_response, Register_device_request,
        Register_device_response, Write_block_request, Write_block_response,
    },
    result_err, result_ok,
};

struct SafeVirtIOBlk(VirtIOBlk<MOSHal, PciTransport>);
unsafe impl Send for SafeVirtIOBlk {}

#[derive(Clone)]
struct BlockServer {
    blockdev_manager: RpcStub,
    devname: String,
    server_name: String,
    blockdev: Arc<Mutex<SafeVirtIOBlk>>,
}

RpcPbServer!(
    BlockServer,
    (1, on_read, (Read_block_request, Read_block_response)),
    (2, on_write, (Write_block_request, Write_block_response))
);

impl BlockServer {
    fn register(&mut self) -> RpcResult<()> {
        let dev = &self.blockdev.lock().unwrap().0;

        let request = Register_device_request {
            server_name: self.server_name.clone(),
            device_info: Some(Blockdev_info {
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
                std::io::ErrorKind::Other,
                format!("failed to register blockdev: {}", resp.result.error),
            )));
        } else {
            Ok(())
        }
    }

    fn on_read(&mut self, req: &Read_block_request) -> Option<Read_block_response> {
        let mut buf = vec![0u8; 512 * req.n_blocks as usize];

        let virtioblk = &mut self.blockdev.lock().unwrap().0;

        let resp = match virtioblk.read_blocks(req.n_boffset as _, &mut buf) {
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

        Some(resp)
    }

    fn on_write(&mut self, req: &Write_block_request) -> Option<Write_block_response> {
        assert!(req.data.len() == 512 * req.n_blocks as usize);

        let virtioblk = &mut self.blockdev.lock().unwrap().0;

        let resp = match virtioblk.write_blocks(req.n_boffset as _, &req.data) {
            Ok(()) => Write_block_response {
                result: result_ok!(),
                ..Default::default()
            },
            Err(e) => Write_block_response {
                result: result_err!(format!("failed to write: {}", e)),
                ..Default::default()
            },
        };

        Some(resp)
    }
}

pub fn run_blockdev(transport: PciTransport, func: DeviceFunction) -> RpcResult<()> {
    let devlocation = format!("{:02x}:{:02x}:{:02x}", func.bus, func.device, func.function);

    let devname = format!("virtblk.{}", devlocation);

    // check /dev/block if the device is already registered
    if std::fs::metadata(format!("/dev/block/{}", devname)).is_ok() {
        println!("blockdev {} already registered", devname);
        return Ok(());
    }

    let server_name = format!("blockdev.virtio.{}", devlocation);
    let mut driver = BlockServer {
        blockdev_manager: RpcStub::new("mos.blockdev-manager")?,
        devname,
        server_name: server_name.clone(),
        blockdev: Arc::new(Mutex::new(SafeVirtIOBlk(
            VirtIOBlk::new(transport).expect("failed to create blockdev"),
        ))),
    };

    driver.register()?;

    let mut rpc_server = RpcPbServer::create(&server_name, Box::new(driver))?;
    libsm_rs::report_service_status(libsm_rs::RpcUnitStatusEnum::Started, "blockdev is online");
    rpc_server.run()
}
