# SPDX-License-Identifier: GPL-3.0-or-later

add_summary_section(ARCH_X86 "x86 Architecture (64-bit)")
add_summary_section(ARCH_X86_DEBUG "x86 Debug")

set(MOS_COMPILER_PREFIX "x86_64-elf-")
set(MOS_CX_FLAGS "${MOS_CX_FLAGS} -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx")
set(MOS_KERNEL_CFLAGS "-mno-red-zone;-mcmodel=kernel")
set(MOS_BITS 64)

mos_kconfig(ARCH_X86    MOS_PAGE_SIZE                   4096                "x86_64 Page Size")
mos_kconfig(ARCH_X86    MOS_ADDR_USER_HEAP              0x0000000040000000  "User Heap Start Address")
mos_kconfig(ARCH_X86    MOS_ADDR_USER_STACK             0x0000000100000000  "User Stack Start Address")
mos_kconfig(ARCH_X86    MOS_ADDR_USER_MMAP              0x0000000C00000000  "User MMAP Start Address")
mos_kconfig(ARCH_X86    MOS_INITRD_VADDR                0xFFFFC8C000000000  "Initrd Virtual Address")
mos_kconfig(ARCH_X86    MOS_HWMEM_VADDR                 0xFFFFFFFFA0000000  "Hardware Virtual Map Address")

mos_kconfig(ARCH_X86_DEBUG X86_DEBUG_ALL OFF "Enable all x86 debug logs")

macro(x86_debug feature description)
    mos_kconfig(ARCH_X86_DEBUG MOS_DEBUG_x86_${feature} ${X86_DEBUG_ALL} ${description})
endmacro()

x86_debug(cpu       "x86 CPU debug log")
x86_debug(lapic     "x86 Local APIC debug log")
x86_debug(ioapic    "x86 I/O APIC debug log")
x86_debug(startup   "x86 Startup debug log")
x86_debug(acpi      "x86 ACPI debug log")
