
# SPDX-License-Identifier: GPL-3.0-or-later

set(MOS_GLOBAL_C_CXX_FLAGS "${MOS_GLOBAL_C_CXX_FLAGS}")

set(_MOS_KERNEL_FLAGS "-mcmodel=medany;-march=rv64gc;-mabi=lp64d;-fno-omit-frame-pointer")
list(APPEND MOS_KERNEL_CFLAGS   "${_MOS_KERNEL_FLAGS}")
list(APPEND MOS_KERNEL_CXXFLAGS "${_MOS_KERNEL_FLAGS}")
set(_MOS_KERNEL_FLAGS)
