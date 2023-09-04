// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/tasks/signal_types.h>
#include <mos/types.h>

int raise(signal_t sig);

int kill(pid_t pid, signal_t sig);

int sigaction(signal_t sig, sighandler *handler);
