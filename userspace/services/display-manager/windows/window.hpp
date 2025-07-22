// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "utils/SubView.hpp"
#include "utils/common.hpp"

#include <mos/types.h>
#include <pb.h>
#include <string>

namespace DisplayManager::Windows
{
    class Window
    {
      public:
        explicit Window(u64 wId, const std::string &title, Point pos, Size size);
        ~Window();

        bool UpdateContent(const Region &local, pb_bytes_array_t *content);

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

      private:
        Window(Window &other) = delete;
        Window &operator=(Window &other) = delete;

      public:
        const u64 windowId; ///< Unique identifier for the window

      private:
        Point position;    ///< Position of the window on the screen
        Size size;         ///< Size of the window
        std::string title; ///< Title of the window

        pb_bytes_array_t *backingBuffer = nullptr;
    };
} // namespace DisplayManager::Windows
