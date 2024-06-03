// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.h>

/**
 * @brief Physical page number of the phyframes array.
 *
 */
extern pfn_t phyframes_pfn;

/**
 * @brief Number of pages required for the phyframes array.
 *
 */
extern size_t phyframes_npages;

void x86_initialise_phyframes_array(void);
