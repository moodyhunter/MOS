// SPDX-License-Identifier: GPL-3.0-or-later

#include <fcntl.h>
#include <readline/libreadline.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFSIZE 4096

void clear_console(void)
{
    printf("\033[2J\033[1;1H");
}

static void print_file(FILE *f)
{
    do
    {
        char buffer[BUFSIZE] = { 0 };
        size_t sz = fread(buffer, 1, BUFSIZE, f);
        if (sz == 0)
            break;

        fwrite(buffer, 1, sz, stdout);
    } while (true);
}

static void open_and_print_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
    {
        fprintf(stderr, "failed to open file '%s'\n", path);
        return;
    }

    print_file(f);
    fclose(f);
}

static void do_pstat(void)
{
    while (true)
    {
        char *line = readline("pfn: ");
        if (!line || strlen(line) == 0)
            return;

        FILE *f = fopen("/sys/mmstat/phyframe_stat", "rw");
        if (!f)
        {
            fprintf(stderr, "failed to open 'phyframe_stat'\n");
            free(line);
            return;
        }

        fprintf(f, "%s\n", line);
        print_file(f);
        fclose(f);
        free(line);
    }
}

static void do_pagetable(void)
{
    while (true)
    {
        char *line = readline("pid: ");
        if (!line || strlen(line) == 0)
            return;

        FILE *f = fopen("/sys/mmstat/pagetable", "rw");
        if (!f)
        {
            fprintf(stderr, "failed to open 'pagetable'\n");
            free(line);
            return;
        }

        fprintf(f, "%s\n", line);
        print_file(f);
        fclose(f);
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
    { "memstat", do_memstat },     //
    { "leave", do_leave },         //
    { "pstat", do_pstat },         //
    { "pagetable", do_pagetable }, //
    { NULL, NULL },                //
};

int main(int argc, const char *argv[])
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);
    puts("KDebug, the MOS kernel debugger.");

    while (true)
    {
        const char *line = readline("kdebug> ");

        bool handled = false;
        for (int i = 0; actions[i].name; i++)
        {
            if (!strcmp(line, actions[i].name))
            {
                handled = true;
                actions[i].func();
                free((void *) line);
                break;
            }
        }

        if (handled)
        {
            if (strlen(line))
                puts("unknown command");
            free((void *) line);
        }
    }

    return 0;
}
