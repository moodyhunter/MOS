// SPDX-License-Identifier: GPL-3.0-or-later

use std::thread;

use crate::{
    IpcChannel, IpcServer, RpcCallArgType, RpcCallResult, RPC_ARG_MAGIC, RPC_REQUEST_MAGIC,
    RPC_RESPONSE_MAGIC,
};

use num_traits::FromPrimitive;

// ! BEGIN public API

pub type RpcResult<T> = std::result::Result<T, Box<dyn std::error::Error>>;

pub struct RpcCallFuncInfo<'a, T: Send + Sync + Clone> {
    pub id: u32,
    pub func: fn(&mut T, &mut RpcCallContext) -> RpcResult<()>,
    pub argtypes: &'a [RpcCallArgType],
}

pub struct RpcCallContext<'a> {
    argtypes: &'a [RpcCallArgType],
    data: Vec<u8>,
    reply: Option<RpcReply>,
}

pub struct RpcServer<'a, T: Send + Sync + Clone> {
    ipc: IpcServer,
    functions: &'a [RpcCallFuncInfo<'a, T>],
}

// ! END public API

macro_rules! get_arg_ints {
    ($name:ident, $int_type:ty) => {
        pub fn $name(&self, index: u32) -> RpcResult<$int_type> {
            let data = self.get_narg(index)?;
            let data = <$int_type>::from_le_bytes(
                data[..std::mem::size_of::<$int_type>()]
                    .try_into()
                    .expect("buffer read overflow"),
            );
            Ok(data)
        }
    };
}

macro_rules! data_slice_and_shift {
    ($data:ident, $newvar:ident, $size:expr) => {
        if $data.len() < $size {
            return Err(Box::new(std::io::Error::new(
                std::io::ErrorKind::InvalidData,
                "invalid data in rpc arg",
            )));
        }
        let $newvar = $data[..$size].to_vec();
        $data = $data[$size..].to_vec();
    };
}

macro_rules! do_try_into {
    ($data:ident, $from:expr, $to:expr) => {
        $data[$from..$to].try_into().or_else(|_| {
            Err(std::io::Error::new(
                std::io::ErrorKind::InvalidData,
                "invalid data in rpc arg",
            ))
        })?
    };
}

macro_rules! new_ioerr {
    ($message:expr) => {
        Box::new(std::io::Error::new(
            std::io::ErrorKind::InvalidData,
            $message,
        ))
    };
}

struct RpcReply(Vec<u8>);

impl<'a> RpcCallContext<'a> {
    fn get_narg(&self, index: u32) -> RpcResult<Vec<u8>> {
        let mut data = self.data.clone();
        for _ in 0..index {
            data_slice_and_shift!(data, this_arg, 12);

            if u32::from_be_bytes(do_try_into!(this_arg, 0, 4)) != RPC_ARG_MAGIC {
                return Err(new_ioerr!("invalid magic in rpc arg"));
            }

            // [4..8] is argtype, unused in this loop, skip
            let size = u32::from_le_bytes(do_try_into!(this_arg, 8, 12)) as usize;
            data_slice_and_shift!(data, _unused, size); // skip data
        }

        data_slice_and_shift!(data, this_arg, 12);

        if u32::from_be_bytes(do_try_into!(this_arg, 0, 4)) != RPC_ARG_MAGIC {
            return Err(new_ioerr!("invalid magic in rpc arg"));
        }

        let argtype = u32::from_le_bytes(do_try_into!(this_arg, 4, 8));
        let argtype = RpcCallArgType::from_u32(argtype).ok_or(new_ioerr!(
            "invalid argtype".to_owned() + argtype.to_string().as_str()
        ))?;
        if self.argtypes[index as usize] != argtype {
            return Err(new_ioerr!("invalid argtype in rpc arg"));
        }

        let size = u32::from_le_bytes(do_try_into!(this_arg, 8, 12)) as usize;
        let argdata = data[..size].to_vec();

        Ok(argdata)
    }

    pub fn get_arg_string(&self, index: u32) -> RpcResult<String> {
        self.get_narg(index).and_then(|data| {
            std::str::from_utf8(&data)
                .and_then(|s| Ok(s.to_string()))
                .or(Err(new_ioerr!("invalid utf8")))
        })
    }

    pub fn get_arg_buffer(&self, index: u32) -> RpcResult<Vec<u8>> {
        self.get_narg(index)
    }

    #[cfg(feature = "protobuf")]
    pub fn get_arg_pb<T: protobuf::Message>(&self, index: u32) -> RpcResult<T> {
        let data = self.get_narg(index)?;
        T::parse_from_bytes(&data).or(Err(new_ioerr!("invalid protobuf")))
    }

    #[cfg(feature = "protobuf")]
    pub fn write_response_pb<T: protobuf::Message>(&mut self, pb: &T) -> RpcResult<()> {
        let mut data = Vec::new();
        pb.write_to_vec(&mut data)
            .or(Err(new_ioerr!("failed to write protobuf")))?;
        self.reply = Some(RpcReply(data));
        Ok(())
    }

    get_arg_ints!(get_arg_i8, i8);
    get_arg_ints!(get_arg_i16, i16);
    get_arg_ints!(get_arg_i32, i32);
    get_arg_ints!(get_arg_i64, i64);
    get_arg_ints!(get_arg_u8, u8);
    get_arg_ints!(get_arg_u16, u16);
    get_arg_ints!(get_arg_u32, u32);
    get_arg_ints!(get_arg_u64, u64);
}

impl<'a, T: Send + Sync + Clone> RpcServer<'a, T> {
    pub fn create(str: &str, functions: &'a [RpcCallFuncInfo<T>]) -> RpcResult<RpcServer<'a, T>> {
        let ipc = IpcServer::create(&str)?;
        let server = RpcServer { ipc, functions };

        #[cfg(feature = "debug")]
        println!("==> rpc server with {} functions", functions.len());

        Ok(server)
    }

    pub fn set_functions(&mut self, functions: &'a [RpcCallFuncInfo<T>]) {
        self.functions = functions;
    }

    pub fn run(&mut self, t: &mut T) -> RpcResult<()> {
        thread::scope(|s| {
            loop {
                let mut t_ = t.clone();
                let functions = self.functions;

                let accept = self.ipc.accept();
                match accept {
                    Ok(Some(mut ipc)) => {
                        #[cfg(feature = "debug")]
                        println!("->> accepted new client");

                        s.spawn(move || {
                            loop {
                                if let Err(err) = Self::handle_call(functions, &mut ipc, &mut t_) {
                                    if let Some(err) = err.downcast_ref::<std::io::Error>() {
                                        if err.kind() == std::io::ErrorKind::UnexpectedEof {
                                            #[cfg(feature = "debug")]
                                            println!("<<- client disconnected");
                                            break; // client disconnected
                                        }
                                    }

                                    println!("handle_call failed: {:?}", err);
                                    break;
                                };
                            }
                        });
                    }
                    Ok(None) => {
                        #[cfg(feature = "debug")]
                        println!("->> server shutting down");
                        break;
                    }
                    Err(e) => {
                        println!("accept failed: {:?}", e);
                        break;
                    }
                };
            }

            #[cfg(feature = "debug")]
            println!("->> waiting for threads to complete");
        });

        Ok(())
    }

    fn handle_call(
        functions: &'a [RpcCallFuncInfo<T>],
        ipc: &mut IpcChannel,
        t: &mut T,
    ) -> RpcResult<()> {
        let mut msg = match ipc.recv_message() {
            Ok(msg) => msg,
            Err(err) => {
                return Err(Box::new(err));
            }
        };

        if msg.len() < 16 {
            unreachable!("message too short");
        }

        data_slice_and_shift!(msg, magic, 4); // magic
        let magic = u32::from_be_bytes(do_try_into!(magic, 0, 4));

        data_slice_and_shift!(msg, call_id, 4); // call_id
        let call_id = u32::from_le_bytes(do_try_into!(call_id, 0, 4));

        data_slice_and_shift!(msg, function_id, 4); // function_id
        let function_id = u32::from_le_bytes(do_try_into!(function_id, 0, 4));

        data_slice_and_shift!(msg, args_count, 4); // args_count
        let args_count = u32::from_le_bytes(do_try_into!(args_count, 0, 4));

        if magic != RPC_REQUEST_MAGIC {
            Self::send_rpc_response(ipc, call_id, RpcCallResult::InvalidArg, None)?;
            return Ok(());
        }

        let funcinfo = match functions.iter().find(|f| f.id == function_id) {
            Some(funcinfo) => funcinfo,
            None => {
                Self::send_rpc_response(ipc, call_id, RpcCallResult::ServerInvalidFunction, None)?;
                return Ok(());
            }
        };

        if funcinfo.argtypes.len() > args_count as usize {
            Self::send_rpc_response(ipc, call_id, RpcCallResult::InvalidArg, None)?;
            #[cfg(feature = "debug")]
            println!(
                "  --> received call for function {} with {} args, but function expects {} args",
                funcinfo.id,
                args_count,
                funcinfo.argtypes.len()
            );
            return Ok(());
        }

        #[cfg(feature = "debug")]
        println!(
            "  --> received call for function {} with {} args",
            funcinfo.id, args_count
        );

        let mut ctx = RpcCallContext {
            argtypes: funcinfo.argtypes,
            data: msg,
            reply: None,
        };

        if let Err(_err) = (funcinfo.func)(t, &mut ctx) {
            #[cfg(feature = "debug")]
            println!("function {} failed: {:?}", funcinfo.id, _err);
            Self::send_rpc_response(ipc, call_id, RpcCallResult::ServerInternalError, None)?;
            return Ok(());
        }

        if let Some(reply) = ctx.reply {
            #[cfg(feature = "debug")]
            println!(
                "  <-- sending reply for function {} with {} bytes",
                funcinfo.id,
                reply.0.len()
            );
            Self::send_rpc_response(ipc, call_id, RpcCallResult::Ok, Some(reply))?;
        } else {
            Self::send_rpc_response(ipc, call_id, RpcCallResult::Ok, None)?; // no reply
        }

        Ok(())
    }

    fn send_rpc_response(
        ipc_channel: &mut IpcChannel,
        call_id: u32,
        result: RpcCallResult,
        data: Option<RpcReply>,
    ) -> RpcResult<()> {
        let mut msg = Vec::new();
        msg.extend_from_slice(&RPC_RESPONSE_MAGIC.to_be_bytes());
        msg.extend_from_slice(&call_id.to_le_bytes());
        msg.extend_from_slice(&(result as u32).to_le_bytes());
        if let Some(data) = data {
            msg.extend_from_slice(&(data.0.len() as usize).to_le_bytes());
            msg.extend_from_slice(&data.0);
        } else {
            msg.extend_from_slice(&(0 as usize).to_le_bytes());
        }

        Ok(ipc_channel.send_message(&msg)?)
    }
}
