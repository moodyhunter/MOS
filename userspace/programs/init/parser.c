// SPDX-License-Identifier: GPL-3.0-or-later

#include "parser.h"

#include "lib/memory.h"
#include "lib/string.h"
#include "mos/filesystem/fs_types.h"
#include "mos/syscall/usermode.h"

typedef struct init_config
{
    size_t count;
    struct _entry
    {
        const char *key;
        const char *value;
    } *entries;
} init_config_t;

init_config_t *config_parse_file(const char *file_path)
{
    file_stat_t stat;
    if (!syscall_file_stat(file_path, &stat))
        return NULL;

    fd_t fd = syscall_file_open(file_path, OPEN_READ);
    if (fd <= 0)
        return NULL;

    char *file_content = malloc(stat.size);

    size_t read = syscall_io_read(fd, file_content, stat.size, 0);
    if (read != stat.size)
        return NULL;

    init_config_t *config = malloc(sizeof(init_config_t));
    config->count = 0;
    config->entries = NULL;

    char *saveptr;
    char *line = strtok_r(file_content, "\n", &saveptr);
    while (line)
    {
        // Skip comments
        if (line[0] == '#')
        {
            line = strtok_r(NULL, "\n", &saveptr);
            continue;
        }

        // Skip empty lines
        if (strlen(line) == 0)
        {
            line = strtok_r(NULL, "\n", &saveptr);
            continue;
        }

        // Parse key
        char *keysaveptr;
        char *key = strtok_r(line, "=", &keysaveptr);
        if (!key)
        {
            line = strtok_r(NULL, "\n", &saveptr);
            continue;
        }

        // Parse value
        char *value = strtok_r(NULL, "=", &keysaveptr);
        if (!value)
        {
            line = strtok_r(NULL, "\n", &saveptr);
            continue;
        }

        // Remove trailing spaces
        size_t key_len = strlen(key);
        while (key[key_len - 1] == ' ')
            key[--key_len] = '\0';

        size_t value_len = strlen(value);
        while (value[value_len - 1] == ' ')
            value[--value_len] = '\0';

        // Remove leading spaces
        while (value[0] == ' ')
            value++;

        // Add entry to the list
        config->entries = realloc(config->entries, sizeof(struct _entry) * (config->count + 1));
        config->entries[config->count].key = strdup(key);
        config->entries[config->count].value = strdup(value);
        config->count++;

        line = strtok_r(NULL, "\n", &saveptr);
    }

    free(file_content);
    return config;
}

const char *config_get(init_config_t *config, const char *key)
{
    for (size_t i = 0; i < config->count; i++)
    {
        if (strcmp(config->entries[i].key, key) == 0)
            return config->entries[i].value;
    }

    return NULL;
}

const char **config_get_all(init_config_t *config, const char *key, size_t *count_out)
{
    size_t count = 0;
    for (size_t i = 0; i < config->count; i++)
    {
        if (strcmp(config->entries[i].key, key) == 0)
            count++;
    }

    if (count == 0)
    {
        if (count_out)
            *count_out = 0;
        return NULL;
    }

    const char **values = malloc(sizeof(char *) * (count + 1));
    size_t index = 0;
    for (size_t i = 0; i < config->count; i++)
    {
        if (strcmp(config->entries[i].key, key) == 0)
            values[index++] = config->entries[i].value;
    }

    values[index] = NULL;
    if (count_out)
        *count_out = count;
    return values;
}
