// SPDX-License-Identifier: GPL-3.0-or-later

#include "syslogd.hpp"

#include <iostream>
#include <stdio.h>
#include <unistd.h>

using enum SyslogLevel;

int main()
{
    do_syslog(Critical, "bad things gonna happen\n");
    do_syslog(Error, "error occurred\n");
    do_syslog(Warning, "this is a warning\n");
    do_syslog(Info, "just some info\n");
    do_syslog(Debug, "debugging info\n");
    do_syslog(Notice, "notice this\n");

    const auto fd = do_open_syslog_fd();
    if (fd < 0)
    {
        puts("Failed to open syslog file descriptor");
        return 1; // Exit with error if syslog file descriptor cannot be opened
    }

    printf("Syslog file descriptor opened successfully: %d\n", fd);
    write(fd, "\1Hello, syslog!\n", 16); // Write a test message to syslog

    fflush(stdout); // Ensure all output is flushed to syslog
    fflush(stderr); // Ensure all error output is flushed to syslog

    std::cout << "Normal print" << std::endl;
    std::cerr << "Error print" << std::endl;
    std::clog << "Clog print" << std::endl;

    return 0;
}
