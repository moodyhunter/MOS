# About MOS

The primary goal of MOS is to provide an platform-independent environment for beginners to learn OS.

They will be able to experiment and get familiar with with several aspects of an operating system:

> Note: The **bold** items are the main objectives of the project.

- Basic OS Knowledges
- Memory Management
  - **Paging**, and a Brief of Segmentation
  - Common Memory Management Practices in Modern Kernels
- File Systems
  - Underlying File Operations
  - VFS Framework (a file-system abstraction layer)
- Process Management
  - Allocating a New Process
  - The Famous `fork()` Syscall
  - **Process Scheduler**
  - **Threads**
  - **Threads Synchronization** via locks/semaphores

Several other features that might be implemented in the future, such as:

- Network Stack (Primarity TCP/IP)
- PCI Support
  - USB Protocol
- GUI Support
  - Graphics Library Support
  - A Graphical Shell

MOS is a learning tool, so adavping MOS onto real hardware is not considered important for now.
This reduces a huge amount of time working on hardware-specific code, e.g. fixing bugs for Specture
CPU bug like what Linux did.
