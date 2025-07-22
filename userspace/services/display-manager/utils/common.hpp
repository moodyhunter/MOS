// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
namespace DisplayManager
{
    struct Point
    {
        int x = 0;
        int y = 0;

        Point ToGlobal(const Point &offset) const
        {
            return { x + offset.x, y + offset.y };
        }

        Point ToLocal(const Point &offset) const
        {
            return { x - offset.x, y - offset.y };
        }

        Point operator-(const Point &other) const
        {
            return { x - other.x, y - other.y };
        }
    };

    struct Size
    {
        int width = 0;
        int height = 0;
    };

    struct Region
    {
        Region(Point p, Size s) : origin(p), size(s) {};
        Point origin; ///< Top-left corner of the region
        Size size;    ///< Size of the region

        Region ToGlobal(const Point &offset) const
        {
            return { origin.ToGlobal(offset), size };
        }

        Region ToLocal(const Point &offset) const
        {
            return { origin.ToLocal(offset), size };
        }

        std::optional<Region> GetIntersection(const Region &other) const
        {
            int x1 = std::max(origin.x, other.origin.x);
            int y1 = std::max(origin.y, other.origin.y);
            int x2 = std::min(origin.x + size.width, other.origin.x + other.size.width);
            int y2 = std::min(origin.y + size.height, other.origin.y + other.size.height);

            if (x1 < x2 && y1 < y2)
                return Region(Point(x1, y1), Size(x2 - x1, y2 - y1));
            return std::nullopt; ///< No intersection
        }
    };
} // namespace DisplayManager
