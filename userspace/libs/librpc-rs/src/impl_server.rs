use std::{io::Error, thread::spawn};

use crate::{
    IpcChannel, IpcServer, RpcCallArgType, RpcCallResult, RPC_ARG_MAGIC, RPC_REQUEST_MAGIC,
    RPC_RESPONSE_MAGIC,
};

use num_traits::FromPrimitive;

// ! BEGIN public API

pub type RpcCallFunction = fn(&mut RpcCallContext) -> Result<(), Error>;

#[derive(Clone, Copy, Debug)]
pub struct RpcCallFuncInfo {
    pub id: u32,
    pub func: RpcCallFunction,
    pub argtypes: &'static [RpcCallArgType],
}

pub struct RpcCallContext {
    funcinfo: RpcCallFuncInfo,
    data: Vec<u8>,
    reply: RpcReply,
}

pub struct RpcServer {
    ipc: IpcServer,
    functions: Vec<RpcCallFuncInfo>,
}

// ! END public API

macro_rules! get_arg_ints {
    ($name:ident, $int_type:ty) => {
        pub fn $name(&self, index: u32) -> Result<$int_type, Error> {
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
            return Err(Error::new(
                std::io::ErrorKind::InvalidData,
                "invalid data in rpc arg",
            ));
        }
        let $newvar = $data[..$size].to_vec();
        $data = $data[$size..].to_vec();
    };
}

macro_rules! do_try_into {
    ($data:ident, $from:expr, $to:expr) => {
        $data[$from..$to].try_into().or_else(|_| {
            Err(Error::new(
                std::io::ErrorKind::InvalidData,
                "invalid data in rpc arg",
            ))
        })?
    };
}

macro_rules! new_ioerr {
    ($message:expr) => {
        Error::new(std::io::ErrorKind::InvalidData, $message)
    };
}

struct RpcReply {
    data: Vec<u8>,
}

impl RpcCallContext {
    fn get_narg(&self, index: u32) -> Result<Vec<u8>, Error> {
        // typedef struct
        // {
        //     u32 magic; // RPC_ARG_MAGIC
        //     u32 argtype;
        //     u32 size;
        //     char data[];
        // } rpc_arg_t;

        let mut data = self.data.clone();
        for _ in 0..index {
            data_slice_and_shift!(data, this_arg, 12);

            if u32::from_le_bytes(do_try_into!(this_arg, 0, 4)) != RPC_ARG_MAGIC {
                return Err(new_ioerr!("invalid magic in rpc arg"));
            }

            // [4..8] is argtype, unused in this loop, skip
            let size = u32::from_le_bytes(do_try_into!(this_arg, 8, 12)) as usize;
            data_slice_and_shift!(data, _unused, size); // skip data
        }

        data_slice_and_shift!(data, this_arg, 12);

        if u32::from_le_bytes(do_try_into!(this_arg, 0, 4)) != RPC_ARG_MAGIC {
            return Err(new_ioerr!("invalid magic in rpc arg"));
        }

        let argtype = u32::from_le_bytes(do_try_into!(this_arg, 4, 8));
        let argtype = RpcCallArgType::from_u32(argtype).ok_or(new_ioerr!("invalid argtype"))?;
        if self.funcinfo.argtypes[index as usize] != argtype {
            return Err(new_ioerr!("invalid argtype in rpc arg"));
        }

        let size = u32::from_le_bytes(do_try_into!(this_arg, 8, 12)) as usize;
        let argdata = data[..size].to_vec();

        Ok(argdata)
    }

    pub fn get_arg_string(&self, index: u32) -> Result<String, Error> {
        self.get_narg(index).and_then(|data| {
            std::str::from_utf8(&data)
                .and_then(|s| Ok(s.to_string()))
                .or(Err(new_ioerr!("invalid utf8")))
        })
    }

    pub fn get_arg_buffer(&self, index: u32) -> Result<Vec<u8>, Error> {
        self.get_narg(index)
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

impl RpcServer {
    pub fn create(str: &str, functions: &[RpcCallFuncInfo]) -> Result<RpcServer, Error> {
        let ipc = IpcServer::create(str)?;
        let server = RpcServer {
            ipc,
            functions: functions.to_vec(),
        };

        #[cfg(feature = "debug")]
        println!("==> rpc server with {} functions", functions.len());

        Ok(server)
    }

    pub fn run(&mut self) -> Result<(), Error> {
        loop {
            let functions = self.functions.clone();
            let mut ipc = match self.ipc.accept()? {
                Some(ipc) => ipc,
                None => return Ok(()),
            };

            #[cfg(feature = "debug")]
            println!("->> accepted new client");

            spawn(move || loop {
                match handle_call(&functions, &mut ipc) {
                    Err(err) => {
                        println!("handle_call failed: {:?}", err);
                        return;
                    }

                    Ok(false) => {
                        #[cfg(feature = "debug")]
                        println!("<<- client disconnected");
                        return; // client disconnected
                    }
                    _ => {}
                }
            });
        }
    }
}

fn handle_call(functions: &Vec<RpcCallFuncInfo>, mut ipc: &mut IpcChannel) -> Result<bool, Error> {
    let mut msg = match ipc.recv_message() {
        Ok(msg) => msg,
        Err(err) => {
            if err.kind() == std::io::ErrorKind::UnexpectedEof {
                return Ok(false);
            }

            return Err(err);
        }
    };

    if msg.len() < 16 {
        unreachable!("message too short");
    }

    // typedef struct
    // {
    //     u32 magic; // RPC_REQUEST_MAGIC
    //     id_t call_id;
    //     u32 function_id;
    //     u32 args_count;
    //     char args_array[]; // rpc_arg_t[]
    // } rpc_request_t;

    data_slice_and_shift!(msg, magic, 4); // magic
    let magic = u32::from_le_bytes(do_try_into!(magic, 0, 4));

    data_slice_and_shift!(msg, call_id, 4); // call_id
    let call_id = u32::from_le_bytes(do_try_into!(call_id, 0, 4));

    data_slice_and_shift!(msg, function_id, 4); // function_id
    let function_id = u32::from_le_bytes(do_try_into!(function_id, 0, 4));

    data_slice_and_shift!(msg, args_count, 4); // args_count
    let args_count = u32::from_le_bytes(do_try_into!(args_count, 0, 4));

    if magic != RPC_REQUEST_MAGIC {
        send_rpc_respose(ipc, call_id, RpcCallResult::InvalidArg, &[])?;
        return Ok(true);
    }

    let funcinfo = match functions.iter().find(|f| f.id == function_id) {
        Some(funcinfo) => funcinfo,
        None => {
            send_rpc_respose(ipc, call_id, RpcCallResult::ServerInvalidFunction, &[])?;
            return Ok(true);
        }
    };

    if funcinfo.argtypes.len() > args_count as usize {
        send_rpc_respose(ipc, call_id, RpcCallResult::InvalidArg, &[])?;
        #[cfg(feature = "debug")]
        println!(
            "  --> received call for function {} with {} args, but function expects {} args",
            funcinfo.id,
            args_count,
            funcinfo.argtypes.len()
        );
        return Ok(true);
    }

    #[cfg(feature = "debug")]
    println!(
        "  --> received call for function {} with {} args",
        funcinfo.id, args_count
    );

    let mut ctx = RpcCallContext {
        funcinfo: funcinfo.clone(),
        data: msg,
        reply: RpcReply { data: Vec::new() },
    };

    if let Err(_err) = (funcinfo.func)(&mut ctx) {
        #[cfg(feature = "debug")]
        println!("function {} failed: {:?}", funcinfo.id, _err);
        send_rpc_respose(&mut ipc, call_id, RpcCallResult::ServerInternalError, &[])?;
        return Ok(true);
    }

    // reply with reply message

    #[cfg(feature = "debug")]
    println!(
        "  <-- sending reply for function {} with {} bytes",
        funcinfo.id,
        ctx.reply.data.len()
    );

    send_rpc_respose(ipc, call_id, RpcCallResult::Ok, &ctx.reply.data)?;
    Ok(true)
}

fn send_rpc_respose(
    ipc_channel: &mut IpcChannel,
    call_id: u32,
    result: RpcCallResult,
    data: &[u8],
) -> Result<(), Error> {
    // typedef struct
    // {
    //     u32 magic; // RPC_RESPONSE_MAGIC
    //     id_t call_id;
    //     rpc_result_code_t result_code;
    //     size_t data_size;
    //     char data[];
    // } rpc_response_t;

    let mut msg = Vec::new();
    msg.extend_from_slice(&RPC_RESPONSE_MAGIC.to_le_bytes());
    msg.extend_from_slice(&call_id.to_le_bytes());
    msg.extend_from_slice(&(result as u32).to_le_bytes());
    msg.extend_from_slice(&(data.len() as usize).to_le_bytes());
    msg.extend_from_slice(data);

    ipc_channel.send_message(&msg)
}
