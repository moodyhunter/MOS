// SPDX-License-Identifier: GPL-3.0-or-later

use std::{
    fs::File,
    io::{Error, ErrorKind::UnexpectedEof, Read, Write},
    os::fd::FromRawFd,
};

// typedef
use i32 as fd_t;

pub struct MosIpcChannel {
    file: std::fs::File,
}

impl MosIpcChannel {
    pub fn connect(str: &str) -> Result<MosIpcChannel, Error> {
        let mut ipc_path = std::path::PathBuf::from("/sys/ipc/");
        ipc_path.push(str);

        let file = std::fs::OpenOptions::new()
            .read(true)
            .write(true)
            .open(ipc_path)?;

        Ok(MosIpcChannel { file })
    }

    pub fn send_message(&mut self, message: &[u8]) -> Result<(), Error> {
        // first write the length of the message
        let len = message.len() as usize;
        self.file.write_all(&len.to_le_bytes())?;
        self.file.write_all(message)?;
        Ok(())
    }

    pub fn recv_message(&mut self) -> Result<Vec<u8>, Error> {
        // first read the length of the message
        let mut len_buf = [0u8; 8];
        self.file.read_exact(&mut len_buf)?;
        let len = usize::from_le_bytes(len_buf);

        // then read the message
        let mut buf = vec![0u8; len as usize];
        self.file.read_exact(&mut buf)?;
        Ok(buf)
    }
}

pub struct MosIpcServer {
    file: std::fs::File,
}

impl MosIpcServer {
    pub fn create(str: &str) -> Result<MosIpcServer, Error> {
        let mut ipc_path = std::path::PathBuf::from("/sys/ipc/");
        ipc_path.push(str);

        let file = std::fs::OpenOptions::new()
            .read(true)
            .write(true)
            .create_new(true)
            .open(ipc_path)?;

        Ok(MosIpcServer { file })
    }

    pub fn accept(&mut self) -> Result<Option<MosIpcChannel>, Error> {
        // for a server, reading results in a new file descriptor being returned
        // this file descriptor is then used to create a new IpcChannelFd

        let mut fd_buf = [0u8; 4];

        if let Err(e) = self.file.read_exact(&mut fd_buf) {
            if e.kind() == UnexpectedEof {
                return Ok(None); // EOF, the server is shutting down
            } else {
                return Err(e);
            }
        }

        let fd = fd_t::from_le_bytes(fd_buf);

        Ok(Some(MosIpcChannel {
            file: unsafe { File::from_raw_fd(fd) },
        }))
    }
}
