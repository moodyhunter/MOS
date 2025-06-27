// SPDX-License-Identifier: GPL-3.0-or-later

use std::sync::{Arc, Mutex};

use crate::{
    mosrpc::net_networkd::{RegisterNetworkDeviceRequest, RegisterNetworkDeviceResponse},
    MOSHal,
};
use librpc_rs::RpcCallResult;
use librpc_rs::RpcPbServerTrait;
use librpc_rs::{RpcPbReply, RpcPbServer, RpcResult, RpcStub};
use virtio_drivers::{
    device::net::VirtIONet,
    transport::pci::{bus::DeviceFunction, PciTransport},
};

struct SafeVirtIONet(VirtIONet<MOSHal, PciTransport, 16>);
unsafe impl Send for SafeVirtIONet {}

struct NetworkDeviceServer {
    networkd: RpcStub,
    devname: String,
    server_name: String,
    device: Arc<Mutex<SafeVirtIONet>>,
}

RpcPbServer!(
    NetworkDeviceServer,
    (
        1,
        stub,
        (RegisterNetworkDeviceRequest, RegisterNetworkDeviceResponse)
    )
);

impl NetworkDeviceServer {
    fn register(&mut self) -> RpcResult<()> {
        let request = RegisterNetworkDeviceRequest {
            device_name: self.devname.clone(),
            rpc_server_name: self.server_name.clone(),
            ..Default::default()
        };

        let resp: RegisterNetworkDeviceResponse = self.networkd.create_pb_call(1, &request)?;

        if !resp.result.success {
            return Err(Box::new(std::io::Error::new(
                std::io::ErrorKind::Other,
                format!("failed to register blockdev: {}", resp.result.error),
            )));
        } else {
            Ok(())
        }
    }

    fn stub(
        &mut self,
        _request: &RegisterNetworkDeviceRequest,
    ) -> Option<RegisterNetworkDeviceResponse> {
        let dev = &self.device.lock().unwrap().0;
        println!("dev.mac: {:?}", dev.mac_address());
        None
    }
}

pub fn run_netdev(transport: PciTransport, func: DeviceFunction) -> RpcResult<()> {
    let mut netdev = VirtIONet::<MOSHal, PciTransport, 16>::new(transport, 4096).unwrap();

    netdev.enable_interrupts();

    let mac = netdev.mac_address();
    let mac = format!(
        "{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
    );

    println!("MAC address: {}", mac);

    let location = format!("{:02x}:{:02x}:{:02x}", func.bus, func.device, func.function);
    let devname = format!("virtnet.{}", location);

    let server_name = format!("netdev.virtio.{}", location);
    let mut driver = NetworkDeviceServer {
        networkd: RpcStub::new("mos.networkd")?,
        devname,
        server_name: server_name.clone(),
        device: Arc::new(Mutex::new(SafeVirtIONet(netdev))),
    };

    driver.register()?;

    let mut rpc_server = RpcPbServer::create(&server_name, Box::new(driver))?;
    libsm_rs::report_service_status(libsm_rs::RpcUnitStatusEnum::Started, "online");
    rpc_server.run()
}
