// SPDX-License-Identifier: GPL-3.0-or-later

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <readline/libreadline.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
        if (sz == 0 || sz == (size_t) -1)
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
static void do_prompt_rw(const char *prompt, const char *filename)
{
    while (true)
    {
        char *line = readline(prompt);
        if (!line || strlen(line) == 0)
        {
            puts("leaving...");
            if (line)
                free(line);
            return;
        }

        FILE *f = fopen(filename, "r+");
        setbuf(f, NULL);
        if (!f)
        {
            fprintf(stderr, "failed to open '%s'\n", filename);
            free(line);
            return;
        }

        fprintf(f, "%s", line);
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
    exit(0);
}

static FILE *open_debug_file(const char *module, const char *mode)
{
    char *path;
    asprintf(&path, "/sys/debug/%s", module);
    FILE *file = fopen(path, mode);
    if (!file)
        fprintf(stderr, "failed to open '%s'\n", path);
    free(path);
    return file;
}

static bool get_debug_value(const char *module)
{
    FILE *file = open_debug_file(module, "r");
    if (!file)
    {
        printf("debug: cannot open '%s': %s\n", module, strerror(errno));
        exit(1);
    }

    int value;
    fscanf(file, "%d", &value);
    fclose(file);
    return value;
}

static bool set_debug_value(const char *module, bool value)
{
    FILE *file = open_debug_file(module, "w");
    if (!file)
    {
        printf("debug: cannot open '%s': %s\n", module, strerror(errno));
        return false;
    }

    fprintf(file, "%d", value);
    fclose(file);
    return true;
}

static void do_help(void);

static void do_pagetable()
{
    do_prompt_rw("pid: ", "/sys/mmstat/pagetable");
}

static void do_vmaps()
{
    do_prompt_rw("pid: ", "/sys/mmstat/vmaps");
}

static void do_pstat(void)
{
    do_prompt_rw("pfn: ", "/sys/mmstat/phyframe_stat");
}

const struct
{
    const char *name;
    void (*func)(void);
} actions[] = {
    { "memstat", do_memstat },     //
    { "q", do_leave },             //
    { "leave", do_leave },         //
    { "pstat", do_pstat },         //
    { "pagetable", do_pagetable }, //
    { "vmaps", do_vmaps },         //
    { "help", do_help },           //
    { "h", do_help },              //
    { NULL, NULL },                //
};

void do_help()
{
    puts("KD, the MOS kernel debugger.");
    puts("Available commands:");
    for (int i = 0; actions[i].name; i++)
        printf("  %s\n", actions[i].name);

    puts("");
    puts("Also, you can use 'kd' to enable/disable debug modules.");
    puts("Usage:");
    puts("  kd -l [<module>]        list all, or get the status of a debug module");
    puts("  kd <module> <on|off>    enable/disable a debug module");
}

int main(int argc, const char *argv[])
{
    if (argc == 2 && strcmp(argv[1], "-h") == 0)
        do_help();

    if (argc == 2 && strcmp(argv[1], "-l") == 0)
    {
        // list all debug modules and their status
        DIR *dir = opendir("/sys/debug");
        if (!dir)
        {
            printf("debug: cannot open '/sys/debug': %s\n", strerror(errno));
            return 1;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)))
        {
            if (entry->d_type != DT_REG)
                continue;

            const bool on = get_debug_value(entry->d_name);
            printf("%s: %s\n", entry->d_name, on ? "on" : "off");
        }

        closedir(dir);
        return 0;
    }

    if (argc == 3)
    {
        if (strcmp(argv[1], "-l") == 0)
        {
            const char *module = argv[2];
            const bool on = get_debug_value(module);
            printf("%s: %s\n", module, on ? "on" : "off");
        }
        else
        {
            const char *module = argv[1], *value = argv[2];

            if (strcmp(value, "on") == 0 || strcmp(value, "1") == 0)
                set_debug_value(module, true);
            else if (strcmp(value, "off") == 0 || strcmp(value, "0") == 0)
                set_debug_value(module, false);
            else
                printf("debug: invalid value '%s'\n", value);
        }
        return 0;
    }

    // interactive mode
    while (true)
    {
        const char *line = readline("kd> ");
        if (!line)
            break;

        if (strlen(line) == 0)
        {
            free((void *) line);
            continue;
        }

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

        if (!handled)
        {
            if (strlen(line))
                puts("unknown command");
            free((void *) line);
        }
    }

    return 0;
}
