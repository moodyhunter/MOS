// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

typedef struct init_config init_config_t;

init_config_t *config_parse_file(const char *file_path);

// returns a pointer to the value of the key, or NULL if the key is not found
// the returned pointer is valid until the config is freed
const char *config_get(init_config_t *config, const char *key);

// returns a malloc'd array of pointers to the values of the key, or NULL if the key is not found
const char **config_get_all(init_config_t *config, const char *key, size_t *count);
