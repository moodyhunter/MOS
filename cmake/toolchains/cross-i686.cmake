# SPDX-License-Identifier: GPL-3.0-or-later

set(CMAKE_SYSTEM_PROCESSOR i686)
set(CMAKE_C_COMPILER "i686-elf-gcc")
set(CMAKE_LUNKER "i686-elf-ld")
list(APPEND CMAKE_EXE_LINKER_FLAGS "-nostdlib -lgcc")
list(APPEND CMAKE_C_FLAGS "-ffreestanding")
