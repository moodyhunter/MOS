# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(cxx_test cxx_test.cpp)
target_compile_options(cxx_test PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti -fno-exceptions>)
setup_userspace_program(cxx_test /tests "C++ test program")
