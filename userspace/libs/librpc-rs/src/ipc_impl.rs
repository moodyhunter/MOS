// SPDX-License-Identifier: GPL-3.0-or-later

use std::io::{Error, Read, Write};

use crate::IPC;

pub(crate) struct IpcFile {
    pub(crate) file: std::fs::File,
}

impl IPC for IpcFile {
    fn send_message(&mut self, message: &[u8]) -> Result<(), Error> {
        // first write the length of the message
        let len = message.len();
        self.file.write_all(&len.to_le_bytes())?;
        self.file.write_all(message)?;
        Ok(())
    }

    fn recv_message(&mut self) -> Result<Vec<u8>, Error> {
        // first read the length of the message
        let mut len_buf = [0u8; 8];
        self.file.read_exact(&mut len_buf)?;
        let len = u64::from_le_bytes(len_buf);

        // then read the message
        let mut buf = vec![0u8; len as usize];
        self.file.read_exact(&mut buf)?;
        Ok(buf)
    }

    fn close(&mut self) -> Result<(), Error> {
        self.file.sync_all()?;
        Ok(())
    }
}
