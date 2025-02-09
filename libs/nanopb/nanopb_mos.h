// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/mos_global.h>
#include <stdint.h>
#include <stddef.h>

#if defined(__MOS_MINIMAL_LIBC__) || defined(__MOS_KERNEL__)
#include <mos_string.hpp>
#include <mos_stdlib.hpp>
#else
#include <string.h>
#include <stdlib.h>
#endif

#define PB_FIELD_32BIT 1
#define PB_VALIDATE_UTF8 1
#define PB_ENABLE_MALLOC 1
#define PB_STATIC_ASSERT(a, b) MOS_STATIC_ASSERT(a, #b);
