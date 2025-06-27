use std::{
    sync::{Arc, Mutex},
    thread,
};

pub use std::result::Result;

use crate::{
    data_slice_and_shift, do_try_into, IpcChannel, IpcServer, RpcCallResult, RpcResult,
    RPC_REQUEST_MAGIC, RPC_RESPONSE_MAGIC,
};

#[macro_export]
macro_rules! RpcPbServer {
    ($name:tt, $(($id:tt, $func:ident, ($treq:ident, $tresp:ident))), *) => {
        impl RpcPbServerTrait for $name {
            fn dispatch(&mut self, function_id: u32, data: &Vec<u8>) -> RpcPbReply {
                match function_id {
                    $(
                        $id => {
                            let req: $treq = protobuf::Message::parse_from_bytes(data)
                                .map_err(|_| RpcCallResult::ServerInternalError)?;
                            let resp: Option<$tresp> = self.$func(&req);
                            match resp {
                                Some(resp) => Ok(protobuf::Message::write_to_bytes(&resp).unwrap()),
                                None => Err(RpcCallResult::ServerInternalError),
                            }
                        },
                    )*
                    _ => Err(RpcCallResult::ServerInvalidFunction),
                }
            }
        }
    };
}

pub type RpcPbReply = Result<Vec<u8>, RpcCallResult>;

pub trait RpcPbServerTrait {
    fn dispatch(&mut self, function_id: u32, data: &Vec<u8>) -> RpcPbReply;
}

pub struct RpcPbServer<T: RpcPbServerTrait + Send> {
    ipc: IpcServer,
    core: Arc<Mutex<Box<T>>>,
}

impl<T: RpcPbServerTrait + Send> RpcPbServer<T> {
    pub fn create(str: &str, core: Box<T>) -> RpcResult<RpcPbServer<T>> {
        let ipc = IpcServer::create(&str)?;
        let server = RpcPbServer {
            ipc,
            core: Arc::new(Mutex::new(core)),
        };

        #[cfg(feature = "debug")]
        println!("==> rpc server");

        Ok(server)
    }

    pub fn run(&mut self) -> RpcResult<()> {
        thread::scope(|s| {
            loop {
                let accept = self.ipc.accept();
                match accept {
                    Ok(Some(mut ipc)) => {
                        #[cfg(feature = "debug")]
                        println!("->> accepted new client");

                        let self_cloned = self.core.clone();

                        s.spawn(move || {
                            loop {
                                if let Err(err) = Self::handle_call(self_cloned.clone(), &mut ipc) {
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

    fn handle_call(core: Arc<Mutex<Box<T>>>, ipc: &mut IpcChannel) -> RpcResult<()> {
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

        if args_count != 1 {
            Self::send_rpc_response(ipc, call_id, RpcCallResult::InvalidArg, None)?;
            #[cfg(feature = "debug")]
            println!(
                "  --> received call for function {} with {} args, but function expects only 1.",
                function_id, args_count,
            );
            return Ok(());
        }

        #[cfg(feature = "debug")]
        {
            println!(
                "  --> received call for function {} with {} args",
                function_id, args_count
            );
            println!(
                "  --> call_id: {}, magic: {}, function_id: {}, args_count: {}",
                call_id, magic, function_id, args_count
            );
            // dump packet data:
            println!(" --> data: {:?}", msg);
        }

        // remove argument header
        msg = msg
            .get((3 * 4)..)
            .ok_or_else(|| {
                std::io::Error::new(
                    std::io::ErrorKind::UnexpectedEof,
                    "message too short after header",
                )
            })?
            .to_vec();

        let result: RpcPbReply;

        {
            let mut core_locked = core.lock().unwrap();
            result = core_locked.dispatch(function_id, &msg);
        }

        match result {
            Err(_err) => {
                #[cfg(feature = "debug")]
                println!("function {} failed: {:?}", function_id, _err);
                Self::send_rpc_response(ipc, call_id, _err, None)?;
                return Ok(());
            }
            Ok(reply) => {
                #[cfg(feature = "debug")]
                println!(
                    "  <-- sending reply for function {} with {} bytes",
                    function_id,
                    reply.len()
                );
                Self::send_rpc_response(ipc, call_id, RpcCallResult::Ok, Some(reply))?;
            }
        }

        Ok(())
    }

    fn send_rpc_response(
        ipc_channel: &mut IpcChannel,
        call_id: u32,
        result: RpcCallResult,
        data: Option<Vec<u8>>,
    ) -> RpcResult<()> {
        let mut msg = Vec::new();
        msg.extend_from_slice(&RPC_RESPONSE_MAGIC.to_be_bytes());
        msg.extend_from_slice(&call_id.to_le_bytes());
        msg.extend_from_slice(&(result as u32).to_le_bytes());
        if let Some(data) = data {
            msg.extend_from_slice(&(data.len() as usize).to_le_bytes());
            msg.extend_from_slice(&data);
        } else {
            msg.extend_from_slice(&(0 as usize).to_le_bytes());
        }

        Ok(ipc_channel.send_message(&msg)?)
    }
}
