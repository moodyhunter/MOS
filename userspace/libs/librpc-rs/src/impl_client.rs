use std::io::{Error, ErrorKind, Read};

use crate::{
    IpcChannel, RpcCallArgStructs, RpcCallArgType, RpcCallResult, RPC_ARG_MAGIC, RPC_REQUEST_MAGIC,
    RPC_RESPONSE_MAGIC,
};

pub struct RpcStubCall<'a> {
    call_id: u32,
    function_id: u32,
    args: Vec<RpcCallArgStructs>,
    channel: &'a mut IpcChannel,
}

#[derive(Clone)]
pub struct RpcStub {
    channel: IpcChannel,
    call_id_seq: u32,
}

impl RpcStub {
    pub fn new(name: &str) -> Result<RpcStub, Error> {
        let channel = IpcChannel::connect(name)?;
        Ok(RpcStub {
            channel,
            call_id_seq: 0,
        })
    }

    pub fn create_call(&mut self, function_id: u32) -> RpcStubCall {
        self.call_id_seq += 1;
        RpcStubCall {
            call_id: self.call_id_seq,
            function_id,
            args: Vec::new(),
            channel: &mut self.channel,
        }
    }

    #[cfg(feature = "protobuf")]
    pub fn create_pb_call<Req: protobuf::Message, Resp: protobuf::Message>(
        self: &mut Self,
        func_id: u32,
        msg: &Req,
    ) -> Result<Resp, std::io::Error> {
        let mut call = self.create_call(func_id);

        call.add_arg(RpcCallArgStructs::Buffer(msg.write_to_bytes()?));

        match call.exec()? {
            (RpcCallResult::Ok, Some(respbuf)) => {
                let mut from_bytes = protobuf::CodedInputStream::from_bytes(&respbuf);
                Ok(Resp::parse_from(&mut from_bytes)?)
            }
            (RpcCallResult::Ok, None) => Err(std::io::Error::new(
                ErrorKind::Other,
                "call failed: no response",
            )),
            (_, _) => Err(std::io::Error::new(
                ErrorKind::Other,
                "call failed: unknown error",
            )),
        }
    }
}

impl RpcStubCall<'_> {
    pub fn add_arg(&mut self, arg: RpcCallArgStructs) -> &mut Self {
        self.args.push(arg);
        self
    }

    pub fn exec(&mut self) -> Result<(RpcCallResult, Option<Vec<u8>>), Error> {
        // typedef struct
        // {
        //     u32 magic; // RPC_ARG_MAGIC
        //     rpc_argtype_t argtype;
        //     u32 size;
        //     char data[];
        // } rpc_arg_t;

        // typedef struct
        // {
        //     u32 magic; // RPC_REQUEST_MAGIC
        //     id_t call_id;

        //     u32 function_id;
        //     u32 args_count;
        //     char args_array[]; // rpc_arg_t[]
        // } rpc_request_t;

        let mut buf = Vec::new();
        buf.extend_from_slice(&RPC_REQUEST_MAGIC.to_be_bytes());
        buf.extend_from_slice(&self.call_id.to_le_bytes());
        buf.extend_from_slice(&self.function_id.to_le_bytes());
        buf.extend_from_slice(&(self.args.len() as u32).to_le_bytes());

        for arg in &self.args {
            buf.extend_from_slice(&RPC_ARG_MAGIC.to_be_bytes());

            macro_rules! marshal {
                ($typeenum:ident, $val:ident) => {
                    buf.extend_from_slice(&(RpcCallArgType::$typeenum as i32).to_le_bytes());
                    let bytes = $val.to_le_bytes();
                    buf.extend_from_slice(&(bytes.len() as u32).to_le_bytes());
                    buf.extend_from_slice(&bytes);
                };
            }

            match arg {
                RpcCallArgStructs::Float32(f) => {
                    marshal!(Float32, f);
                }
                RpcCallArgStructs::Float64(f) => {
                    marshal!(Float64, f);
                }
                RpcCallArgStructs::Int8(i) => {
                    marshal!(Int8, i);
                }
                RpcCallArgStructs::Int16(i) => {
                    marshal!(Int16, i);
                }
                RpcCallArgStructs::Int32(i) => {
                    marshal!(Int32, i);
                }
                RpcCallArgStructs::Int64(i) => {
                    marshal!(Int64, i);
                }
                RpcCallArgStructs::Uint8(i) => {
                    marshal!(Uint8, i);
                }
                RpcCallArgStructs::Uint16(i) => {
                    marshal!(Uint16, i);
                }
                RpcCallArgStructs::Uint32(i) => {
                    marshal!(Uint32, i);
                }
                RpcCallArgStructs::Uint64(i) => {
                    marshal!(Uint64, i);
                }
                RpcCallArgStructs::String(s) => {
                    buf.extend_from_slice(&(RpcCallArgType::String as i32).to_le_bytes());
                    buf.extend_from_slice(&((s.len() + 1) as u32).to_le_bytes());
                    buf.extend_from_slice(s.as_bytes()); // also send the null terminator
                    buf.push(0);
                }
                RpcCallArgStructs::Buffer(innerbuf) => {
                    buf.extend_from_slice(&(RpcCallArgType::Buffer as i32).to_le_bytes());
                    buf.extend_from_slice(&(innerbuf.len() as u32).to_le_bytes());
                    buf.extend_from_slice(innerbuf);
                }
            }
        }

        #[cfg(feature = "debug")]
        println!(
            "--> sending call for function {} with {} args: {:?}",
            self.function_id,
            self.args.len(),
            buf
        );

        self.channel.send_message(&buf)?;

        let result = self.channel.recv_message()?;

        // typedef struct
        // {
        //     u32 magic; // RPC_RESPONSE_MAGIC
        //     id_t call_id;

        //     rpc_result_code_t result_code;
        //     size_t data_size;
        //     char data[];
        // } rpc_response_t;

        let mut result = result.as_slice();
        let mut magic = [0u8; 4];
        result.read_exact(&mut magic)?;
        let magic = u32::from_be_bytes(magic);
        if magic != RPC_RESPONSE_MAGIC {
            return Err(Error::new(
                ErrorKind::InvalidData,
                "invalid magic in rpc response",
            ));
        }

        let mut call_id = [0u8; 4];
        result.read_exact(&mut call_id)?;
        let call_id = u32::from_le_bytes(call_id);
        if call_id != self.call_id {
            return Err(Error::new(
                ErrorKind::InvalidData,
                "invalid call id in rpc response",
            ));
        }

        let mut result_code = [0u8; 4];
        result.read_exact(&mut result_code)?;
        let result_code = u32::from_le_bytes(result_code);
        let result_code = match result_code {
            0 => RpcCallResult::Ok,
            1 => RpcCallResult::InvalidArg,
            2 => RpcCallResult::ServerInvalidFunction,
            3 => RpcCallResult::ServerInternalError,
            _ => return Err(Error::new(ErrorKind::InvalidData, "invalid result code")),
        };

        let mut data_size = [0u8; 8]; // u64
        result.read_exact(&mut data_size)?;
        let data_size = u64::from_le_bytes(data_size);
        if data_size == 0 {
            return Ok((result_code, None));
        }

        let mut data = vec![0u8; data_size as usize];
        result.read_exact(&mut data)?;

        self.args.clear();

        Ok((result_code, Some(data)))
    }
}
