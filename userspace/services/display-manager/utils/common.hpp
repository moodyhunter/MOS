// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <ostream>
namespace DisplayManager
{
    struct Delta
    {
        int x = 0;
        int y = 0;

        Delta operator+(const Delta &other) const
        {
            return { x + other.x, y + other.y };
        }

        Delta operator-(const Delta &other) const
        {
            return { x - other.x, y - other.y };
        }

        operator bool() const
        {
            return x != 0 || y != 0; // Non-zero delta indicates movement
        }
    };

    struct Region;

    struct Size
    {
        int width = 0;
        int height = 0;
    };

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

        Delta operator-(const Point &other) const
        {
            return { x - other.x, y - other.y };
        }

        Point Clamped(const Region &region) const;
        Point Clamped(const Size &size) const
        {
            return {
                std::max(0, std::min(x, size.width - 1)),
                std::max(0, std::min(y, size.height - 1)),
            };
        }
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

        bool InRegion(const Point &point) const
        {
            return point.x >= origin.x && point.x < origin.x + size.width && point.y >= origin.y && point.y < origin.y + size.height;
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

        void Clip(const Region &clipRegion)
        {
            auto intersection = GetIntersection(clipRegion);
            if (intersection)
            {
                origin = intersection->origin;
                size = intersection->size;
            }
            else
            {
                size = { 0, 0 }; // No intersection, effectively empty region
            }
        }

        Region GetUnion(const Region &other) const
        {
            int x1 = std::min(origin.x, other.origin.x);
            int y1 = std::min(origin.y, other.origin.y);
            int x2 = std::max(origin.x + size.width, other.origin.x + other.size.width);
            int y2 = std::max(origin.y + size.height, other.origin.y + other.size.height);

            return Region(Point(x1, y1), Size(x2 - x1, y2 - y1));
        }
    };

    inline Point Point::Clamped(const Region &region) const
    {
        return { std::max(region.origin.x, std::min(x, region.origin.x + region.size.width - 1)),
                 std::max(region.origin.y, std::min(y, region.origin.y + region.size.height - 1)) };
    }

} // namespace DisplayManager

inline std::ostream &operator<<(std::ostream &os, const DisplayManager::Point &p)
{
    return os << "(" << p.x << ", " << p.y << ")";
}

inline std::ostream &operator<<(std::ostream &os, const DisplayManager::Size &s)
{
    return os << "[" << s.width << "x" << s.height << "]";
}

inline std::ostream &operator<<(std::ostream &os, const DisplayManager::Region &r)
{
    return os << "Region(" << r.origin << ", " << r.size << ")";
}
