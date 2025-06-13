mod mosrpc;

use std::sync::{Mutex, OnceLock};

use librpc_rs::RpcStub;
use mosrpc::services::{RpcUnitStatus, UnitStateNotifyRequest, UnitStateNotifyResponse};
use protobuf::{EnumOrUnknown, MessageField};

pub use mosrpc::services::RpcUnitStatusEnum;

const UNIT_STATE_RECEIVER_SERVICE: &str = "mos.service_manager.unit_state_receiver";

fn get_service_manager() -> &'static Mutex<RpcStub> {
    static SERVICE_MANAGER: OnceLock<Mutex<RpcStub>> = OnceLock::new();
    SERVICE_MANAGER.get_or_init(|| Mutex::new(RpcStub::new(UNIT_STATE_RECEIVER_SERVICE).unwrap()))
}

pub fn report_service_status_with_token(
    status_enum: RpcUnitStatusEnum,
    message: &str,
    token: &str,
) -> Result<(), Box<dyn std::error::Error>> {
    let req = UnitStateNotifyRequest {
        service_id: token.to_string(),
        status: MessageField::some(RpcUnitStatus {
            isActive: status_enum != RpcUnitStatusEnum::Stopped,
            status: EnumOrUnknown::new(status_enum),
            statusMessage: message.to_string(),
            ..Default::default()
        }),
        ..Default::default()
    };

    let resp: UnitStateNotifyResponse = get_service_manager()
        .lock()
        .unwrap()
        .create_pb_call(1, &req)?;

    eprintln!(
        "ReportServiceStatus: {:?}",
        if resp.success { "Success" } else { "Failed" }
    );

    Ok(())
}

pub fn report_service_status(status_enum: RpcUnitStatusEnum, message: &str) {
    let pid = std::process::id();
    match std::env::var("MOS_SERVICE_TOKEN") {
        Ok(token) => match report_service_status_with_token(status_enum, message, &token) {
            Ok(_) => (),
            Err(e) => eprintln!("libsm-rs@{}: Failed to report service status: {}", pid, e),
        },
        Err(_) => eprintln!("libsm-rs@{}: MOS_SERVICE_TOKEN not set", pid),
    }
}
