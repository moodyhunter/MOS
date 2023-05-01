# MOS

This is the source code for MOS, an operating system written in C for educational purposes.

For a detailed description of setting up the development environment, see the attached PDF
named `lab-tutorial.pdf`.

## File Structure

```txt
.
├── arch
│   └── x86
│       ├── acpi            ; x86 ACPI table parsing
│       ├── boot            ; x86 boot code
│       │   ├── multiboot   ;  - Multiboot-compatible startup code
│       │   └── multiboot_iso
│       ├── cpu             ; x86 CPU code, CPUID, SMP, etc.
│       ├── descriptors     ; x86 GDT, IDT, TSS, etc.
│       ├── devices         ; x86 device drivers, serial, initrd block device, etc.
│       ├── include         ; x86 architecture specific includes
│       │   ├── private/... ;  - Private includes
│       │   └── public/...  ;  - Public includes, system call implementations
│       ├── interrupt       ; x86 interrupt handlers, PIC, IDT, LAPIC, IOAPIC, etc.
│       ├── mm              ; x86 memory management, paging, page tables
│       └── tasks           ; x86 context switching, TSS, etc.
├── assets
│   ├── imgs
│   └── logo
├── cmake                   ; CMake helper modules
├── docs                    ; several documentation files (cmdline, cmake etc.)
├── kernel
│   ├── device              ; Generic block device & console layer
│   ├── elf                 ; ELF executable loader, (static linked only)
│   ├── filesystem          ; Virtual filesystem layer
│   │   ├── cpio            ;  - CPIO filesystem, used for initrd
│   │   ├── ipcfs           ;  - IPC filesystem, lists IPC channels
│   │   └── tmpfs           ;  - TMPFS filesystem, as a demo filesystem
│   ├── include
│   │   ├── private/...     ; Private includes
│   │   └── public/mos      ; Public includes, available to userspace
│   │       ├── device
│   │       ├── filesystem
│   │       ├── io
│   │       ├── mm
│   │       └── tasks
│   ├── interrupt           ; Inter-processor interrupts, currently unuesd
│   ├── io                  ; IO subsystem, generic IO api
│   ├── ipc                 ; IPC subsystem
│   ├── locks               ; Futex implementation
│   ├── mm                  ; Memory management subsystem, Copy-on-write
│   │   ├── ipcshm          ;  - IPC shared memory
│   │   ├── paging          ;  - Paging subsystem, page allocator
│   │   └── physical        ;  - Physical memory allocator
│   └── tasks               ; Process/thread management, scheduler
├── libs                    ; Libraries used by both kernel and userspace
│   ├── libipc              ;  - IPC library
│   ├── librpc              ;  - RPC library
│   └── stdlib              ;  - Standard library, (`libc`)
│       ├── structures      ;  - linked list, stack, hashmap, bitmap, etc.
│       └── sync            ;  - spinlock, mutex
├── scripts
├── tests                   ; Kernel-space unit tests
│   ├── cmdline_parser
│   ├── downwards_stack
│   ├── hashmap
│   ├── kmalloc
│   ├── linked_list
│   ├── memops
│   ├── printf
│   └── ring_buffer
└── userspace               ; Userspace programs
    ├── cmake               ; Userspace CMake helper modules
    ├── drivers
    │   ├── device-manager  ; Device manager daemon, currently unused
    │   ├── pci-daemon      ; PCI daemon, currently unused
    │   └── x86-console     ; x86 text-mode console driver
    ├── include             ; user-space standard library includes
    ├── initrd              ; initial ramdisk static contents
    │   ├── assets          ;  - assets
    │   └── config          ;  - `init` and device manager config files
    ├── labs                ; lab assignments
    │   └── answers         ;  - lab answers
    ├── libs
    │   ├── argparse        ; Argument parser library
    │   ├── libconfig       ; Configuration file parser library
    │   └── readline        ; Readline library
    ├── programs            ; userspace programs
    │   ├── 2048
    │   ├── hello
    │   ├── init
    │   ├── lazybox
    │   └── mossh
    └── tests               ; Userspace unit tests
        ├── cxx
        ├── fork
        ├── ipc
        └── librpc
```

## License & Credits

MOS is licensed under the GPLv3 license, or (at your option) any later version.

### Credits

- [OSDev](https://wiki.osdev.org/Main_Page) for the great tutorials
- [newlib](https://sourceware.org/newlib/) for the `memmove` implementation
- [multiboot](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html) for the multiboot specification
- [GCC/Binutils/GDB](https://gcc.gnu.org/) for the compiler, linker, assembler and debugger
- [ELF](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format) for the executable format
- [liballoc](https://github.com/blanham/liballoc) for the kernel and userspace heap allocator
- [u-boot](https://github.com/u-boot/) for the 2048 game 😆
