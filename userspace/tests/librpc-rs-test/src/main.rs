// SPDX-License-Identifier: GPL-3.0-or-later

use std::io::Error;

use librpc_rs::{
    define_rpc_server, rpc_server_function, rpc_server_stub_function, IpcChannel, IpcServer,
    RpcCallArgStructs, RpcCallContext, RpcCallFuncInfo, RpcResult, RpcServer, RpcStub,
};

#[derive(Clone)]
struct MyServerImpl {}

impl MyServerImpl {
    fn on_echo(&mut self, ctx: &mut RpcCallContext) -> RpcResult<()> {
        println!("1. {}", ctx.get_arg_string(0)?);
        println!("2. {}", ctx.get_arg_string(1)?);
        println!("3. {}", ctx.get_arg_string(2)?);
        println!("4. {}", ctx.get_arg_i64(3)?);
        Ok(())
    }

    fn on_print(&mut self, ctx: &mut RpcCallContext) -> RpcResult<()> {
        println!("1. {}", ctx.get_arg_string(0)?);
        Ok(())
    }

    fn on_something(&mut self, ctx: &mut RpcCallContext) -> RpcResult<()> {
        println!("1. {}", ctx.get_arg_i32(0)?);
        Ok(())
    }
}

define_rpc_server!(pub, MyServer);

impl MyServer {
    rpc_server_stub_function!(
        0,
        echo,
        (str, &str, String, to_string),
        (str2, String, String),
        (str3, String, String),
        (val, i64, Int64)
    );
    rpc_server_stub_function!(1, print, (str, &str, String, to_string));
    rpc_server_stub_function!(2, something, (val, i32, Int32));
}

const FUNCTIONS: &[RpcCallFuncInfo<MyServerImpl>] = &[
    rpc_server_function!(0, MyServerImpl::on_echo, String, String, String, Int64),
    rpc_server_function!(1, MyServerImpl::on_print, String),
    rpc_server_function!(2, MyServerImpl::on_something, Int32),
];

fn main() -> RpcResult<()> {
    if let Some(arg) = std::env::args().nth(1) {
        if arg == "ipc-server" {
            let mut server = IpcServer::create("echo-server")?;

            loop {
                let mut chan = match server.accept() {
                    Ok(Some(chan)) => chan,
                    Ok(None) => {
                        println!("No more connections");
                        break;
                    }
                    Err(err) => {
                        println!("Error: {}", err);
                        break;
                    }
                };

                let msg = chan.recv_message()?;
                println!("Received message: {:?}", msg);
                chan.send_message(&msg)?;
            }

            return Ok(());
        } else if arg == "ipc-client" {
            let mut ipc = IpcChannel::connect("echo-server")?;
            ipc.send_message(b"Hello, world!")?;
            let msg = ipc.recv_message()?;

            // print message as string
            let msg = std::str::from_utf8(&msg).expect("invalid utf8, this should not happen");
            println!("Received message: {:}", msg);
            return Ok(());
        } else if arg == "rpc-server" {
            let mut rpc_server = RpcServer::create("librpc-rs-demo", FUNCTIONS)?;

            let mut xserver = MyServerImpl {};
            match rpc_server.run(&mut xserver) {
                Ok(_) => {}
                Err(err) => {
                    println!("Error: {}", err);
                }
            }
            return Ok(());
        } else if arg == "rpc-client" {
            let mut client = MyServer::new("librpc-rs-demo")?;
            client
                .echo("Hello, world!", "A".to_string(), "B".to_string(), 1234_i64)
                .expect("echo failed");
            client.print("Hello, world!").expect("print failed");
            return Ok(());
        }
    }

    println!("Usage: librpc-rs-test <ipc-echo|ipc-server|rpc-server|rpc-client>");
    Ok(())
}
