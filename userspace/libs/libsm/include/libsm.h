// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/mos_global.h>

enum class UnitStatus
{
    Starting = 0,
    Started = 1,
    Failed = 2,
    Stopping = 3,
    Stopped = 4,
};

MOSAPI bool ReportServiceState(UnitStatus status, const char *message);
