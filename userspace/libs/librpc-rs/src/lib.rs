// SPDX-License-Identifier: GPL-3.0-or-later
#![feature(fn_traits)]

// ! IPC Server and Channels
pub(crate) mod ipc_impl;
pub use ipc_impl::*; // import all IPC implementations (for MOS and Unix)

macro_rules! make_4cc {
    ($a:tt, $b:tt, $c:tt, $d:tt) => {
        (($a as u32) << 24) | (($b as u32) << 16) | (($c as u32) << 8) | ($d as u32)
    };
}

pub(crate) const RPC_REQUEST_MAGIC: u32 = make_4cc!('R', 'P', 'C', '>');
pub(crate) const RPC_RESPONSE_MAGIC: u32 = make_4cc!('R', 'P', 'C', '<');
pub(crate) const RPC_ARG_MAGIC: u32 = make_4cc!('R', 'P', 'C', 'A');

// ! RPC Servers
pub(crate) mod impl_server;
pub use impl_server::{RpcCallContext, RpcCallFuncInfo, RpcResult, RpcServer};

#[macro_export]
macro_rules! rpc_server_function {
    ($id:tt, $func:expr,  $($argtypes:tt),*) => {
        librpc_rs::RpcCallFuncInfo {
            id: $id,
            func: $func,
            argtypes: &[$(librpc_rs::RpcCallArgType::$argtypes),*],
        }
    };
}

// ! RPC Clients
pub(crate) mod impl_client;
pub use impl_client::RpcStub;

#[macro_export]
macro_rules! define_rpc_server {
    ($visiblity:tt, $servername:ident) => {
        $visiblity struct $servername {
            rpc_server: RpcStub,
        }

        impl $servername {
            pub fn new(name: &str) -> Result<Self, Error> {
                let stub = RpcStub::new(name)?;
                Ok($servername {
                    rpc_server: stub,
                })
            }
        }
    };
}

#[macro_export]
macro_rules! rpc_server_stub_function {
    ($func_id:expr, $name:ident, $(($argname:ident, $argtype:ty, $argtypenum:ident $(, $argtypetofn:ident)?)),*) => {
        pub fn $name(&mut self, $($argname: $argtype),*) -> Result<(librpc_rs::RpcCallResult, Option<Vec<u8>>), std::io::Error> {
            self.rpc_server
                .create_call($func_id)
                $(.add_arg(RpcCallArgStructs::$argtypenum($argname $(.$argtypetofn())?)))*
                .exec()
        }
    };
}

// !! Keep these 3 enums in-sync with the C version !!
#[derive(Clone, Debug, num_derive::FromPrimitive, PartialEq)]
pub enum RpcCallResult {
    Ok = 0,
    ServerInvalidFunction,
    ServerInvalidArgCount,
    ServerInternalError,
    InvalidArg,
    ClientInvalidArgspec,
    ClientWriteFailed,
    ClientReadFailed,
    CallidMismatch,
}

#[derive(Clone, Debug, num_derive::FromPrimitive, PartialEq)]
pub enum RpcCallArgType {
    Float32,
    Int8,
    Float64,
    Int16,
    Int32,
    Int64,
    Uint8,
    Uint16,
    Uint32,
    Uint64,
    String,
    Buffer,
}

#[derive(Clone, Debug, PartialEq)]
pub enum RpcCallArgStructs {
    Float32(f32),
    Float64(f64),
    Int8(i8),
    Int16(i16),
    Int32(i32),
    Int64(i64),
    Uint8(u8),
    Uint16(u16),
    Uint32(u32),
    Uint64(u64),
    String(String),
    Buffer(Vec<u8>),
}
