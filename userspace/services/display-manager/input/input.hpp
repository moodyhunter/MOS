// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "utils/common.hpp"
namespace DisplayManager::Input
{
    enum class MouseEventType
    {
        MouseMove,
        MouseClick,
        MouseRelease,
        MouseScroll
    };

    struct MouseEvent
    {
        MouseEventType type; // Type of mouse event
        bool leftButton;
        bool rightButton;
        bool middleButton;

        Point cursorPosition;
        Delta movement; // Movement delta from the last event
    };

    bool InitializeInputd();
} // namespace DisplayManager::Input
