// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "mos/platform/platform.h"

#include <mos/mos_global.h>
#include <mos/types.h>

typedef u64 pf_point_t;

#if MOS_CONFIG(MOS_PROFILING)

/**
 * @brief Enter a profiling scope
 *
 * @param name Scope name
 * @return id_t The scope ID used to exit the scope
 */
should_inline pf_point_t profile_enter(void)
{
    return platform_get_timestamp();
}

/**
 * @brief Exit a profiling scope
 *
 * @param id Scope ID
 */
void profile_leave(pf_point_t point, const char *fmt, ...) __printf(2, 3);

#else

#define profile_enter() 0

#define profile_leave(p, ...) ((void) p)

#endif
