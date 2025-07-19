// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <sys/types.h>
#include <vector>

namespace ExecUtils
{
    /**
     * @brief Redirect stdout to syslog daemon
     */
    void RedirectLogFd(const std::string &unitBase, const std::string &fileName);

    std::string GetRandomString(size_t length = 32);

    /**
     * @brief Do fork and execute the given command, with logging redirected to syslog daemon.
     * and the MOS_SERVICE_TOKEN environment variable set to the given token.
     *
     * @param exec the command to execute, as a vector of strings
     * @param token the token to set in the MOS_SERVICE_TOKEN environment variable
     * @param baseId the base ID of the unit, used to create the log directory
     * @return pid_t the PID of the child process, or -1 on error
     */
    pid_t DoFork(const std::vector<std::string> &exec, const std::string &token, const std::string &baseId, bool redirect = true);
} // namespace ExecUtils
