// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/platform/platform.h>
#include <mos/types.h>

typedef enum
{
    INIT_TARGET_EARLY,
    INIT_TARGET_VFS,
    INIT_TARGET_KTHREAD,
} init_target_t;

typedef struct
{
    const char *param;
    bool (*setup_fn)(const char *arg);
} mos_setup_t;

typedef struct
{
    init_target_t target;
    void (*init_fn)(void);
} mos_init_t;

#define MOS_EARLY_SETUP(_param, _fn) MOS_PUT_IN_SECTION(".mos.early_setup", mos_setup_t, __setup_##_fn, { .param = _param, .setup_fn = _fn })
#define MOS_SETUP(_param, _fn)       MOS_PUT_IN_SECTION(".mos.setup", mos_setup_t, __setup_##_fn, { .param = _param, .setup_fn = _fn })
#define MOS_INIT(_comp, _fn)         MOS_PUT_IN_SECTION(".mos.init", mos_init_t, __init_##_fn, { .target = INIT_TARGET_##_comp, .init_fn = _fn })

void setup_invoke_setup(void);
void setup_invoke_earlysetup(void);
void setup_reach_init_target(init_target_t target);
