// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "utils/common.hpp"
namespace DisplayManager::Input
{
    struct MouseEvent
    {
        bool leftButton;
        bool rightButton;
        bool middleButton;

        Point cursorPosition;
        Delta movement; // Movement delta from the last event
    };

    bool InitializeInputd();
} // namespace DisplayManager::Input
