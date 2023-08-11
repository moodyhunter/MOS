// SPDX-License-Identifier: GPL-3.0-or-later

#include <fcntl.h>
#include <readline/libreadline.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFSIZE 4096

static void clear_console(void)
{
    printf("\033[2J\033[1;1H");
}

void open_and_print_file(const char *path)
{
    fd_t fd = open(path, OPEN_READ);
    if (fd < 0)
    {
        fprintf(stderr, "failed to open file '%s'\n", path);
        return;
    }

    do
    {
        char buffer[BUFSIZE] = { 0 };
        size_t sz = syscall_io_read(fd, buffer, BUFSIZE);
        if (sz == 0)
            break;

        fwrite(buffer, 1, sz, stdout);
    } while (true);

    syscall_io_close(fd);
}

static void do_pstat(void)
{
    char buffer[BUFSIZE] = { 0 };
    while (true)
    {
        char *line = readline("pfn: ");
        if (!line || strlen(line) == 0)
            return;

        clear_console();
        FILE *f = fopen("/sys/mmstat/phyframe_stat", "rw");
        if (!f)
        {
            fprintf(stderr, "failed to open 'phyframe_stat'\n");
            free(line);
            return;
        }

        fprintf(f, "%s\n", line);
        const size_t sz = fread(buffer, 1, BUFSIZE, f);
        fclose(f);

        if (sz == 0)
        {
            free(line);
            continue;
        }

        fwrite(buffer, 1, sz, stdout);
        free(line);
    }
}

static void do_memstat(void)
{
    open_and_print_file("/sys/mmstat/stat");
}

static void do_leave(void)
{
    syscall_exit(0);
}

const struct
{
    const char *name;
    void (*func)(void);
} actions[] = {
    { "memstat", do_memstat },
    { "leave", do_leave },
    { "pstat", do_pstat },
    { NULL, NULL },
};

int main(int argc, const char *argv[])
{
    puts("KDebug, the MOS kernel debugger.");

    while (true)
    {
        const char *line = readline("kdebug> ");
        for (int i = 0; actions[i].name; i++)
        {
            if (!strcmp(line, actions[i].name))
            {
                actions[i].func();
                free((void *) line);
                break;
            }
        }
    }

    return 0;
}
