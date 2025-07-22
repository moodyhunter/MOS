// SPDX-License-Identifier: GPL-3.0-or-later

use std::sync::{Arc, Mutex};

use librpc_rs::RpcCallResult;
use librpc_rs::RpcPbReply;
use librpc_rs::RpcPbServerTrait;
use librpc_rs::{RpcPbServer, RpcResult};
use libsm_rs::result_ok;
use protobuf::MessageField;
use virtio_drivers::{
    device::gpu::VirtIOGpu,
    transport::pci::{bus::DeviceFunction, PciTransport},
};

use crate::mosrpc::graphics_gpu::MoveCursorRequest;
use crate::mosrpc::graphics_gpu::MoveCursorResponse;
use crate::{
    hal::MOSHal,
    mosrpc::graphics_gpu::{
        PostBufferRequest, PostBufferResponse, QueryDisplayInfoRequest, QueryDisplayInfoResponse,
    },
    result_err,
};

struct SafeVirtIOGpu(VirtIOGpu<MOSHal, PciTransport>);
unsafe impl Send for SafeVirtIOGpu {}

#[derive(Clone, Debug, Default)]
struct Size {
    width: u32,
    height: u32,
}

#[derive(Clone)]
struct GpuServer {
    devname: String,
    server_name: String,
    gpu: Arc<Mutex<SafeVirtIOGpu>>,
    framebuffer: *mut u8,
    resolution: Size, // (width, height)
}

unsafe impl Send for GpuServer {}

#[rustfmt::skip]
RpcPbServer!(
    GpuServer,
    (1, on_query_display_info, (QueryDisplayInfoRequest, QueryDisplayInfoResponse)),
    (2, on_post_buffer, (PostBufferRequest, PostBufferResponse)),
    (3, on_move_cursor, (MoveCursorRequest, MoveCursorResponse))
);

impl GpuServer {
    fn new(devname: String, server_name: String, mut gpu: VirtIOGpu<MOSHal, PciTransport>) -> Self {
        let resolution = gpu.resolution().expect("failed to get resolution");
        let framebuffer = gpu
            .setup_framebuffer()
            .expect("failed to setup framebuffer")
            .as_mut_ptr();

        let cursor_image: Vec<u8> = vec![128; 64 * 64 * 4]; // Assuming a 16x16 cursor with 4 bytes per pixel (RGBA)

        gpu.setup_cursor(cursor_image.as_slice(), 64, 64, 64, 64)
            .expect("failed to setup cursor image");

        let server = GpuServer {
            devname,
            server_name,
            gpu: Arc::new(Mutex::new(SafeVirtIOGpu(gpu))),
            framebuffer,
            resolution: Size {
                width: resolution.0,
                height: resolution.1,
            },
        };

        log::info!(
            "GPU server created: devname={}, server_name={}, resolution={:?}",
            server.devname,
            server.server_name,
            server.resolution
        );

        server
    }

    fn get_framebuffer_size(&self) -> u32 {
        return self.resolution.width * self.resolution.height * 4; // Assuming 4 bytes per pixel (RGBA)
    }

    fn get_framebuffer(&mut self) -> &mut [u8] {
        return unsafe {
            std::slice::from_raw_parts_mut(self.framebuffer, self.get_framebuffer_size() as usize)
        };
    }

    fn on_query_display_info(
        &self,
        request: &QueryDisplayInfoRequest,
    ) -> Option<QueryDisplayInfoResponse> {
        log::info!(
            "QueryDisplayInfo: display_name={}, width={}, height={}",
            request.display_name,
            self.resolution.width,
            self.resolution.height
        );

        Some(QueryDisplayInfoResponse {
            display_name: request.display_name.clone(),
            width: self.resolution.width,
            height: self.resolution.height,
            ..Default::default()
        })
    }

    fn on_post_buffer(&mut self, request: &PostBufferRequest) -> Option<PostBufferResponse> {
        let region = request.region.clone();

        let expected_size = region.w as usize * region.h as usize * 4; // Assuming 4 bytes per pixel (RGBA)
        if request.buffer_data.len() != expected_size {
            let errinfo = format!(
                "Buffer data size ({}) does not match expected size ({}) for region {:?}",
                request.buffer_data.len(),
                expected_size,
                region.0.unwrap()
            );
            return Some(PostBufferResponse {
                result: result_err!(errinfo),
                ..Default::default()
            });
        }

        let screen_width = self.resolution.width;
        let screen_height = self.resolution.height;

        // range check
        if region.x + region.w > screen_width || region.y + region.h > screen_height {
            let errinfo = format!(
                "Region {:?} is out of bounds for resolution {:?}",
                region, self.resolution
            );
            return Some(PostBufferResponse {
                result: result_err!(errinfo),
                ..Default::default()
            });
        }

        let fb = self.get_framebuffer();
        // write a subwindow of the framebuffer
        for y in 0..region.h {
            let src_start = (y * region.w) * 4;
            let dst_start = ((region.y + y) * screen_width + region.x) * 4;
            let dst_end = dst_start + (region.w * 4);
            let src_end = src_start + (region.w * 4);

            if dst_end as usize > fb.len() {
                return Some(PostBufferResponse {
                    result: result_err!("Buffer overflow detected"),
                    ..Default::default()
                });
            }

            fb[dst_start as usize..dst_end as usize]
                .copy_from_slice(&request.buffer_data[src_start as usize..src_end as usize]);
        }

        self.gpu.lock().unwrap().0.flush().expect("failed to flush");
        Some(PostBufferResponse {
            result: result_ok!(),
            ..Default::default()
        })
    }

    fn on_move_cursor(&self, request: &MoveCursorRequest) -> Option<MoveCursorResponse> {
        log::info!("MoveCursor: x={}, y={}", request.x, request.y);

        self.gpu
            .lock()
            .unwrap()
            .0
            .move_cursor(request.x, request.y)
            .expect("failed to move cursor");

        Some(MoveCursorResponse {
            result: result_ok!(),
            ..Default::default()
        })
    }
}

pub fn run_gpu(transport: PciTransport, func: DeviceFunction) -> RpcResult<()> {
    let gpu =
        VirtIOGpu::<MOSHal, PciTransport>::new(transport).expect("failed to create gpu driver");

    let location = format!("{:02x}:{:02x}:{:02x}", func.bus, func.device, func.function);
    let devname = format!("virtgpu.{}", location);

    let server_name = format!("gpu.virtio");
    let driver = GpuServer::new(devname, server_name.clone(), gpu);

    let mut rpc_server = RpcPbServer::create(&server_name, Box::new(driver))?;
    libsm_rs::report_service_status(libsm_rs::RpcUnitStatusEnum::Started, "gpu-online");
    rpc_server.run()
}
