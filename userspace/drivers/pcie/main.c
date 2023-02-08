// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/memory.h"
#include "mos/syscall/usermode.h"

#if defined(__i386__) || defined(i386) || defined(__i386) || defined(__i486__) || defined(__i586__) || defined(__i686__)
#define PCIe_IS_X86 1
#else
#error "Unsupported architecture"
#endif

int main(int argc, const char *argv[])
{
#if PCIe_IS_X86

#endif
}
