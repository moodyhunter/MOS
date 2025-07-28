// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "utils/common.hpp"

#include <pb.h>
#include <stdexcept>

namespace DisplayManager::Utils
{
    /**
     * @brief Provides a subwindow view of a larger buffer representing a window's content.
     *
     */
    template<typename E>
    struct SubView
    {
        explicit SubView(void *buffer, size_t bufSize, Size bufferSize, Region subregion)
            : buffer(buffer), bufSize(bufSize), bufferSize(bufferSize), origin(subregion.origin), size(subregion.size)
        {
            if (subregion.origin.x < 0 || subregion.origin.y < 0 || subregion.origin.x + subregion.size.width > bufferSize.width ||
                subregion.origin.y + subregion.size.height > bufferSize.height)
            {
                throw std::out_of_range("Subregion is out of bounds of the original buffer");
            }
        }

        E &operator[](this auto &self, const Point &p)
        {
            if (p.x < 0 || p.y < 0 || p.x >= self.size.width || p.y >= self.size.height)
            {
                throw std::out_of_range("Point is out of bounds of the subwindow");
            }
            return static_cast<E *>(self.buffer)[(self.origin.y + p.y) * self.bufferSize.width + (self.origin.x + p.x)];
        }

      private:
        void *const buffer;    ///< Pointer to the original buffer containing the window content
        const size_t bufSize;  ///< Size of the original buffer in bytes
        const Size bufferSize; ///< Size of the original buffer, in widths and heights

      public:
        const Point origin; ///< Origin of the subwindow in the original buffer
        const Size size;    ///< Size of the subwindow
    };
} // namespace DisplayManager::Utils
