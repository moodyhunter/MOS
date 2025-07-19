// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils/ExecUtils.hpp"

#include "global.hpp"
#include "mos/syscall/usermode.h"

#include <fcntl.h>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

namespace ExecUtils
{
    void RedirectLogFd(const std::string &unitBase, const std::string &fileName)
    {
        MOS_UNUSED(unitBase);
        MOS_UNUSED(fileName);

        fd_t log_fd = 0;
        log_fd = syscall_kmod_call("syslogd", "open_syslogfd", NULL, 0);
        if (log_fd < 0)
        {
            std::cerr << "RedirectLogFd: failed to open syslog file descriptor" << std::endl;
            exit(1);
        }

        dup2(log_fd, STDOUT_FILENO);
        dup2(log_fd, STDERR_FILENO);
        close(log_fd);
    }

    std::string GetRandomString(size_t length)
    {
        static const char alphanum[] = "0123456789"
                                       "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                       "abcdefghijklmnopqrstuvwxyz";
        std::string s;
        s.reserve(length);
        for (size_t i = 0; i < length; i++)
            s.push_back(alphanum[rand() % (sizeof(alphanum) - 1)]);
        return s;
    }

    pid_t DoFork(const std::vector<std::string> &exec, const std::string &token, const std::string &baseId, bool redirect)
    {
        int fds[2];
        if (pipe(fds) == -1)
        {
            std::cerr << RED("failed to create pipe") << std::endl;
            return -1;
        }

        pid_t pid = fork();
        if (pid == 0)
        {
            // child write to pipe a status code, so close the read end
            close(fds[0]);
            fcntl(fds[1], F_SETFD, FD_CLOEXEC);

            std::vector<const char *> args;
            for (const auto &arg : exec)
                args.push_back(arg.c_str());
            args.push_back(nullptr);

            if (redirect)
                ExecUtils::RedirectLogFd(baseId, std::to_string(getpid()));

            setenv("MOS_SERVICE_TOKEN", token.c_str(), true);

            const auto err = execve(exec[0].c_str(), (char **) args.data(), environ);
            if (err == -1)
            {
                std::cerr << RED("failed to execute") << " " << exec[0] << ": " << strerror(errno) << std::endl;
                // write error code to pipe
                int error_code = errno;
                write(fds[1], &error_code, sizeof(error_code));
                close(fds[1]);
                _exit(1);
            }
            // if execve was successful, we should never reach here
            std::cerr << RED("unreachable code") << std::endl;
            __builtin_unreachable();
            return false;
        }

        // parent process read from pipe to get the status code
        close(fds[1]);

        int status_code = 0;
        if (const auto ret = read(fds[0], &status_code, sizeof(status_code)); ret == 0)
        {
            // child process has closed the pipe, which means it has executed successfully
            close(fds[0]);
            return pid;
        }
        else
        {
            close(fds[0]);
            std::cerr << RED("failed to start process") << " " << exec[0] << ": " << strerror(status_code) << std::to_string(status_code) << std::endl;
            return status_code;
        }
    }
} // namespace ExecUtils
