// SPDX-License-Identifier: GPL-3.0-or-later

#include <fcntl.h>
#include <mos/io/io_types.h>
#include <mos/mm/mm_types.h>
#include <stdio.h>
#include <string.h>

#define BUF_SIZE 1024

static char parent_buf[BUF_SIZE] = { 0 };
static char child_buf[BUF_SIZE] = { 0 };

static void read_and_print_file(fd_t fd, char *buf, size_t buf_size)
{
    ssize_t read = syscall_io_read(fd, buf, buf_size);
    if (read < 0)
    {
        puts("Failed to read file");
        syscall_exit(1);
    }

    puts("File contents:");
    puts("========================================");
    puts(buf);
    puts("========================================");
}

static bool test_read_file(void)
{
    // test opened files
    puts("Opening file");
    int fd = open("/initrd/README.txt", OPEN_READ);
    if (fd < 0)
    {
        puts("Failed to open file");
        return 1;
    }

    read_and_print_file(fd, parent_buf, BUF_SIZE);

    // now seek back to the beginning, fork, and read again
    puts("Seeking back to beginning");
    syscall_io_seek(fd, 0, IO_SEEK_SET);

    pid_t child = syscall_fork();

    if (child == 0)
    {
        puts("I am the child, reading file");
        read_and_print_file(fd, child_buf, BUF_SIZE);

        // compare the buffers
        if (strncmp(parent_buf, child_buf, BUF_SIZE) != 0)
        {
            puts("!!!!!!!!!!!!!!!!!!!!");
            puts("Buffers do not match");
            puts("!!!!!!!!!!!!!!!!!!!!");
            return false;
        }

        return true;
    }
    else if (child > 0)
    {
        puts("I am the parent, waiting for child");
        syscall_wait_for_process(child);
        puts("Child exited");
        return true;
    }
    else
    {
        puts("fork failed");
        return false;
    }
}

static bool test_mmap(void)
{
    void *shared = syscall_mmap_anonymous(0, MOS_PAGE_SIZE, MEM_PERM_READ | MEM_PERM_WRITE, MMAP_SHARED);
    if (shared == NULL)
    {
        puts("Failed to mmap anonymous");
        return false;
    }

    // write to the shared memory
    strcpy(shared, "Hello from parent");

    pid_t child = syscall_fork();

    if (child == 0)
    {
        puts("I am the child, reading shared memory");
        puts(shared);

        // compare the buffers
        if (strncmp(shared, "Hello from parent", MOS_PAGE_SIZE) != 0)
        {
            puts("!!!!!!!!!!!!!!!!!!!!");
            puts("Buffers do not match");
            puts("!!!!!!!!!!!!!!!!!!!!");
            return false;
        }

        // write to the shared memory
        strcpy(shared, "Hello from child");

        return true;
    }
    else if (child > 0)
    {
        puts("I am the parent, waiting for child");
        syscall_wait_for_process(child);

        // compare the buffers
        if (strncmp(shared, "Hello from child", MOS_PAGE_SIZE) != 0)
        {
            puts("!!!!!!!!!!!!!!!!!!!!");
            puts("Buffers do not match");
            puts("!!!!!!!!!!!!!!!!!!!!");
            return false;
        }

        puts("Child exited");
        return true;
    }
    else
    {
        puts("fork failed");
        return false;
    }
}

int main(void)
{
    puts("Lab 2 Test Utility");

    // test fork
    pid_t pid = syscall_fork();
    if (pid == 0)
    {
        puts("I am the child");
        return 0;
    }
    else if (pid > 0)
    {
        puts("I am the parent, waiting for child");
        syscall_wait_for_process(pid);
        puts("Child exited");
    }
    else
    {
        puts("fork failed");
        return 1;
    }

    // test read file
    if (test_read_file())
    {
        puts("Test passed");
    }
    else
    {
        puts("Test failed");
        return 1;
    }

    // test mmap
    if (test_mmap())
    {
        puts("Test passed");
    }
    else
    {
        puts("Test failed");
        return 1;
    }

    puts("Done");
    if (syscall_get_pid() == 1)
        syscall_poweroff(false, MOS_FOURCC('G', 'B', 'y', 'e'));

    return 0;
}
