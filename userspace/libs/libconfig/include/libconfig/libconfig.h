// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.h>

typedef struct struct_config config_t;

MOSAPI const config_t *config_parse_file(const char *file_path);

// returns a pointer to the value of the key, or NULL if the key is not found
// the returned pointer is valid until the config is freed
MOSAPI const char *config_get(const config_t *config, const char *key);

// returns a malloc'd array of pointers to the values of the key, or NULL if the key is not found
MOSAPI const char **config_get_all(const config_t *config, const char *key, size_t *count);
