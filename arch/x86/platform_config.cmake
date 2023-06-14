# SPDX-License-Identifier: GPL-3.0-or-later

add_summary_section(ARCH_X86 "x86 Architecture")
add_summary_section(ARCH_X86_DEBUG "x86 Debug")

mos_kconfig(ARCH_X86 MOS_X86_64 OFF "x86_64 Architecture")

set(MOS_CX_FLAGS "${MOS_CX_FLAGS} -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx")

if (MOS_X86_64)
    set(MOS_COMPILER_PREFIX "x86_64-elf-")
    set(MOS_CX_FLAGS "${MOS_CX_FLAGS} -mcmodel=kernel")
    set(MOS_BITS 64)

    mos_kconfig(ARCH_X86    MOS_PAGE_SIZE                   4096                "x86_64 Page Size")
    mos_kconfig(ARCH_X86    MOS_ADDR_USER_HEAP              0x0000000040000000  "User Heap Start Address")
    mos_kconfig(ARCH_X86    MOS_ADDR_USER_STACK             0x0000000100000000  "User Stack Start Address")
    mos_kconfig(ARCH_X86    MOS_ADDR_USER_MMAP              0x0000000C00000000  "User MMAP Start Address")
    # userspace memory ends at 0x00007fffffffffff

    # ...
    # ... non-canonical hole ...
    # ...

    # kernel memory starts at  0xFFFF800000000000
    mos_kconfig(ARCH_X86    MOS_KERNEL_START_VADDR          0xFFFF800000000000  "Kernel Start Virtual Address")
    # 4-level paging supports up to 64 TiB physical memory, this region goes up to 0xFFFFC88000000000
    mos_kconfig(ARCH_X86    MOS_DIRECT_MAP_VADDR            0xFFFF888000000000  "Virt->Phys 1:1 Direct Map Address")
    mos_kconfig(ARCH_X86    MOS_DIRECT_MAP_VADDR_END        0xFFFFC88000000000  "Virt->Phys 1:1 Direct Map Address - End")
    # hole
    mos_kconfig(ARCH_X86    MOS_INITRD_VADDR                0xFFFFC8C000000000  "Initrd Virtual Address")
    # hole
    # 1 TiB kernel heap is way enough for now
    mos_kconfig(ARCH_X86    MOS_ADDR_KERNEL_HEAP            0xFFFFD00000000000  "Kernel Heap Start Address")
    mos_kconfig(ARCH_X86    MOS_ADDR_KERNEL_HEAP_END        0xFFFFD10000000000  "Kernel Heap End Address")
    # hole
    mos_kconfig(ARCH_X86    MOS_PHYFRAME_ARRAY_VADDR        0xFFFFEA0000000000  "Physical Frame Array Virtual Address")
    mos_kconfig(ARCH_X86    MOS_HWMEM_VADDR                 0xFFFFFFFFA0000000  "Hardware Virtual Map Address")
else()
    set(MOS_COMPILER_PREFIX "i686-elf-")
    set(MOS_BITS 32)

    mos_kconfig(ARCH_X86    MOS_PAGE_SIZE                   4096                "x86 Page Size")
    mos_kconfig(ARCH_X86    MOS_ADDR_USER_HEAP              0x40000000          "User Heap Start Address")
    mos_kconfig(ARCH_X86    MOS_ADDR_USER_STACK             0x60000000          "User Stack Start Address")
    mos_kconfig(ARCH_X86    MOS_ADDR_USER_MMAP              0x90000000          "User MMAP Start Address")
    mos_kconfig(ARCH_X86    MOS_ADDR_KERNEL_HEAP            0xC5000000          "Kernel Heap Start Address")
    mos_kconfig(ARCH_X86    MOS_ADDR_KERNEL_HEAP_END        0xF8000000          "Kernel Heap End Address")
    mos_kconfig(ARCH_X86    MOS_INITRD_VADDR                0xF8000000          "Initrd Virtual Address")
    mos_kconfig(ARCH_X86    MOS_PHYFRAME_ARRAY_VADDR        0xFA000000          "Physical Frame Array Virtual Address")
    mos_kconfig(ARCH_X86    MOS_HWMEM_VADDR                 0xFC000000          "Hardware Virtual Map Address")
    mos_kconfig(ARCH_X86    MOS_X86_INITIAL_STACK_SIZE      0x100000            "Initial Stack Size")   # Update corresponding linker script if changed
    mos_kconfig(ARCH_X86    MOS_KERNEL_START_VADDR          0xC0000000          "Kernel start virtual address")
endif()

mos_kconfig(ARCH_X86_DEBUG X86_DEBUG_ALL OFF "Enable all x86 debug logs")

macro(x86_debug feature description)
    mos_kconfig(ARCH_X86_DEBUG MOS_DEBUG_x86_${feature} ${X86_DEBUG_ALL} ${description})
endmacro()

x86_debug(cpu       "x86 CPU debug log")
x86_debug(lapic     "x86 Local APIC debug log")
x86_debug(ioapic    "x86 I/O APIC debug log")
x86_debug(startup   "x86 Startup debug log")
x86_debug(acpi      "x86 ACPI debug log")
