include!(concat!(env!("OUT_DIR"), "/protos/mod.rs"));

#[macro_export]
macro_rules! result_ok {
    () => {
        MessageField::from(Some(crate::mos_rpc::mos_rpc::Result {
            success: true,
            ..Default::default()
        }))
    };
}

#[macro_export]
macro_rules! result_err {
    ($msg:expr) => {
        MessageField::from(Some(crate::mos_rpc::mos_rpc::Result {
            success: false,
            error: Some($msg.to_string()),
            ..Default::default()
        }))
    };
}
