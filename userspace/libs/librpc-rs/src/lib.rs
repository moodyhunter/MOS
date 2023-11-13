// SPDX-License-Identifier: GPL-3.0-or-later

use std::io::Error;

mod ipc_impl;

pub trait IPC {
    fn send_message(&mut self, message: &[u8]) -> Result<(), Error>;
    fn recv_message(&mut self) -> Result<Vec<u8>, Error>;
    fn close(&mut self) -> Result<(), Error>;
}

pub fn ipc_connect(server: &str) -> Result<Box<dyn IPC>, Error> {
    let mut ipc_path = std::path::PathBuf::from("/sys/ipc/");
    ipc_path.push(server);

    if !ipc_path.exists() {
        return Err(Error::new(
            std::io::ErrorKind::NotFound,
            "IPC server does not exist",
        ));
    }

    let file = std::fs::OpenOptions::new()
        .read(true)
        .write(true)
        .append(false)
        .truncate(false)
        .open(ipc_path)?;

    Ok(Box::new(ipc_impl::IpcFile { file }))
}
