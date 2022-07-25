# SPDX-License-Identifier: GPL-3.0-or-later

set(CMAKE_SYSTEM_PROCESSOR i686)
set(CMAKE_SYSTEM_NAME Linux)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffreestanding")

set(CMAKE_EXE_LINKER_FLAGS "-nostdlib")

# For the C compiler, 'elf' should be in the OS field for target triplet to produce a bare metal binary.
# Although the `linux-gnu` targeted gcc 'does' work, it's highly unsuggested.

find_program(CC_PATH NAMES "i686-elf-gcc" "i686-linux-gnu-gcc")
if(CC_PATH)
    message(STATUS "TOOLCHAIN: Found i686 C: ${CC_PATH}")
    set(CMAKE_C_COMPILER ${CC_PATH})
else()
    message(FATAL_ERROR "TOOLCHAIN: Could not find i686 C compiler")
endif()

# find_program(CXX_PATH NAMES "i686-elf-g++" "i686-linux-gnu-g++")
# if(CXX_PATH)
#     message(STATUS "TOOLCHAIN: Found i686 C++ compiler: ${CXX_PATH}")
#     set(CMAKE_CXX_COMPILER ${CXX_PATH})
# else()
#     message(FATAL_ERROR "TOOLCHAIN: Could not find i686 C++ compiler")
# endif()
