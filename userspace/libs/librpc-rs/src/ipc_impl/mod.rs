// SPDX-License-Identifier: GPL-3.0-or-later

pub mod mos;
pub mod unix;

#[cfg(target_os = "mos")]
pub use mos::MosIpcChannel as IpcChannel;
#[cfg(target_os = "mos")]
pub use mos::MosIpcServer as IpcServer;

#[cfg(not(target_os = "mos"))]
pub use unix::UnixIpcChannel as IpcChannel;
#[cfg(not(target_os = "mos"))]
pub use unix::UnixIpcServer as IpcServer;
