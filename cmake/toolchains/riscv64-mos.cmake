# SPDX-License-Identifier: GPL-3.0-or-later OR MIT

set(CMAKE_SYSROOT "/opt/riscv64-mos")
set(CMAKE_PREFIX_PATH "/opt/riscv64-mos")

set(CMAKE_SYSTEM_NAME "MOS")
set(CMAKE_SYSTEM_PROCESSOR "riscv64")
set(CMAKE_C_COMPILER "riscv64-mos-gcc")
set(CMAKE_CXX_COMPILER "riscv64-mos-g++")
set(CMAKE_ASM_COMPILER "riscv64-mos-gcc")
set(CMAKE_C_COMPILER_TARGET "riscv64-mos")
set(CMAKE_CXX_COMPILER_TARGET "riscv64-mos")
set(CMAKE_ASM_COMPILER_TARGET "riscv64-mos")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)
set(CMAKE_ASM_COMPILER_WORKS 1)
