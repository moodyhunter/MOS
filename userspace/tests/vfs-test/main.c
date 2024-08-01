// SPDX-License-Identifier: GPL-3.0-or-later

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(int, const char *[])
{
    const int fd = open("testfile1", O_CREAT | O_RDWR | O_EXCL); // exclusively create a file
    printf("fd: %d\n", fd);
    close(fd);

    // then we unlink it
    unlink("testfile1");
}
