// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "input/input.hpp"
#include "utils/SubView.hpp"
#include "utils/common.hpp"

#include <list>
#include <mos/types.h>
#include <pb.h>
#include <shared_mutex>
#include <string>

namespace DisplayManager::Windows
{
    enum class WindowType
    {
        Regular,    ///< Regular window type
        Cursor,     ///< Window type for mouse cursor
        Background, ///< Window type for background
        Desktop,    ///< Window type for desktop (taskbars, icons etc)
    };

    class Window
    {
        friend class WindowManagerClass;

      public:
        explicit Window(u64 wId, const std::string &title, Point pos, Size size, WindowType = WindowType::Regular);
        ~Window();

        bool UpdateContent(const Region &local, const void *data, size_t size);

        bool GetRegionContent(const Region &local, Utils::SubView<uint32_t> &destination);

        Point GetPosition() const
        {
            return position;
        }

        Region GetWindowRegion() const
        {
            return Region(position, size);
        }

        pb_bytes_array_t *GetBackingBuffer() const
        {
            return backingBuffer;
        }

        Input::MouseEvent WaitForMouseEvent();

      public:
        bool HandleMouseEvent(const Input::MouseEvent &event);

      private:
        Window(Window &other) = delete;
        Window &operator=(Window &other) = delete;

      public:
        const u64 windowId;          ///< Unique identifier for the window
        const WindowType windowType; ///< Type of the window (Regular, Mouse, etc.)

      private:
        std::mutex mutex;
        std::condition_variable cv; ///< Condition variable for mouse events
        std::list<Input::MouseEvent> events;
        bool isLeftButtonPressed = false;

      private:
        Point position;    ///< Position of the window on the screen
        Size size;         ///< Size of the window
        std::string title; ///< Title of the window

        pb_bytes_array_t *backingBuffer = nullptr;
    };
} // namespace DisplayManager::Windows
