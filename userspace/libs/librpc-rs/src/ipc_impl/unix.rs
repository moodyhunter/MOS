use std::{
    fs::remove_file,
    io::{Error, Read, Write},
    os::unix::net::{UnixListener, UnixStream},
};

pub struct UnixIpcChannel {
    stream: UnixStream,
}

pub struct UnixIpcServer {
    listener: UnixListener,
}

impl UnixIpcChannel {
    pub fn connect(str: &str) -> Result<UnixIpcChannel, Error> {
        let mut ipc_path = std::path::PathBuf::from("/tmp/");
        ipc_path.push(str);

        let stream = UnixStream::connect(ipc_path)?;
        Ok(UnixIpcChannel { stream })
    }

    pub fn send_message(&mut self, message: &[u8]) -> Result<(), Error> {
        // first write the length of the message
        let len = message.len() as usize;
        self.stream.write_all(&len.to_le_bytes())?;
        self.stream.write_all(message)?;
        Ok(())
    }

    pub fn recv_message(&mut self) -> Result<Vec<u8>, Error> {
        // first read the length of the message
        let mut len_buf = [0u8; 8];
        self.stream.read_exact(&mut len_buf)?;
        let len = usize::from_le_bytes(len_buf);

        // then read the message
        let mut buf = vec![0u8; len as usize];
        self.stream.read_exact(&mut buf)?;
        Ok(buf)
    }
}

impl UnixIpcServer {
    pub fn create(str: &str) -> Result<UnixIpcServer, Error> {
        let mut ipc_path = std::path::PathBuf::from("/tmp/");
        ipc_path.push(str);

        remove_file(&ipc_path).ok();

        let listener = UnixListener::bind(ipc_path)?;

        Ok(UnixIpcServer { listener })
    }

    pub fn accept(&mut self) -> Result<Option<UnixIpcChannel>, Error> {
        // for a server, reading results in a new file descriptor being returned
        // this file descriptor is then used to create a new IpcChannelFd

        let (stream, _) = self.listener.accept()?;
        Ok(Some(UnixIpcChannel { stream }))
    }
}
