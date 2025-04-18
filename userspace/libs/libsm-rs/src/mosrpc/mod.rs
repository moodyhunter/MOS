include!(concat!(env!("OUT_DIR"), "/protos/mod.rs"));

#[macro_export]
macro_rules! result_ok {
    () => {
        MessageField::from(Some(crate::mosrpc::mosrpc::Result {
            success: true,
            ..Default::default()
        }))
    };
}

#[macro_export]
macro_rules! result_err {
    ($msg:expr) => {
        MessageField::from(Some(crate::mosrpc::mosrpc::Result {
            success: false,
            error: $msg.to_string(),
            ..Default::default()
        }))
    };
}
