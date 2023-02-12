// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

typedef struct x86_pg_infra_t x86_pg_infra_t;

extern x86_pg_infra_t *const x86_kpg_infra;

void x86_mm_paging_init(void);
void x86_mm_enable_paging(void);
void x86_mm_dump_page_table(x86_pg_infra_t *pg);
