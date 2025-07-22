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
        explicit SubView(pb_bytes_array_t *buffer, Size bufferSize, Region subregion)
            : buffer(buffer), bufferSize(bufferSize), origin(subregion.origin), size(subregion.size)
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
            return reinterpret_cast<E *>(self.buffer->bytes)[(self.origin.y + p.y) * self.bufferSize.width + (self.origin.x + p.x)];
        }

      private:
        pb_bytes_array_t *const buffer; ///< Pointer to the original buffer containing the window content
        const Size bufferSize;          ///< Size of the original buffer

      public:
        const Point origin; ///< Origin of the subwindow in the original buffer
        const Size size;    ///< Size of the subwindow
    };
} // namespace DisplayManager::Utils
