use librpc_rs::ipc_connect;

fn main() -> Result<(), std::io::Error> {
    let mut ipc = ipc_connect("echo-server")?;
    ipc.send_message(b"Hello, world!")?;
    let msg = ipc.recv_message()?;

    // print message as string
    let msg = std::str::from_utf8(&msg).unwrap();
    println!("Received message: {:}", msg);
    ipc.close()?;

    Ok(())
}
