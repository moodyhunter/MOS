// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/memory.h"
#include "lib/string.h"
#include "mos/mos_global.h"
#include "mos/syscall/usermode.h"

/*
 * Configuration file format:
 *
 * # This is a comment
 * key = value # comment
 * key2 = value2
 * key3 = value3
 *
 */

typedef struct
{
    char *key;
    char *value;
} config_entry_t;

static size_t config_entries_count = 0;
static config_entry_t *config_entries = NULL;

static char file_content[4 KB] = { 0 };

bool parse_config_file(fd_t fd)
{
    size_t read = syscall_io_read(fd, file_content, 4 KB, 0);
    if (read == 0)
        return false;

    char *line = strtok(file_content, "\n");
    while (line)
    {
        // Skip comments
        if (line[0] == '#')
        {
            line = strtok(NULL, "\n");
            continue;
        }

        // Skip empty lines
        if (strlen(line) == 0)
        {
            line = strtok(NULL, "\n");
            continue;
        }

        // Parse key
        char *key = strtok(line, "=");
        if (!key)
        {
            line = strtok(NULL, "\n");
            continue;
        }

        // Parse value
        char *value = strtok(NULL, "=");
        if (!value)
        {
            line = strtok(NULL, "\n");
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
        config_entries = realloc(config_entries, sizeof(config_entry_t) * (config_entries_count + 1));
        config_entries[config_entries_count].key = strdup(key);
        config_entries[config_entries_count].value = strdup(value);
        config_entries_count++;

        line = strtok(NULL, "\n");
    }

    return true;
}

int main(int argc, char *argv[])
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);

    // We don't have a console yet, so printing to stdout will not work.
    fd_t config_file_fd = syscall_file_open("/assets/config/init.conf", FILE_OPEN_READ);

    if (config_file_fd <= 0)
        return 1;

    bool parsed = parse_config_file(config_file_fd);

    if (!parsed)
        return 2;

    syscall_io_close(config_file_fd);

    return 0;
}
