// SPDX-License-Identifier: GPL-3.0-or-later

#include <cassert>
#include <mos/types.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int main(void)
{
    fd_t fd = memfd_create("name", MFD_CLOEXEC);

    const int writtensize = write(fd, "Hello, World!", 13);
    assert(writtensize == 13);

    const int pos = lseek(fd, 0, SEEK_SET);
    assert(pos == 0);

    char buf[13];
    const int readsize = read(fd, buf, 13);
    assert(readsize == 13);

    const int cmp = memcmp(buf, "Hello, World!", 13);
    assert(cmp == 0);
}
