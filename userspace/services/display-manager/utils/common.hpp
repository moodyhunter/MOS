// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <ostream>
namespace DisplayManager
{
    struct Delta
    {
        int x = 0;
        int y = 0;

        Delta operator+(const Delta &other) const;

        Delta operator-(const Delta &other) const;

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

        Point ToGlobal(const Point &offset) const;

        Point ToLocal(const Point &offset) const;

        Delta operator-(const Point &other) const;

        Point Clamped(const Region &region) const;

        Point Clamped(const Size &size) const;
    };

    struct Region
    {
        Region(Point p, Size s) : origin(p), size(s) {};
        Point origin; ///< Top-left corner of the region
        Size size;    ///< Size of the region

        Region ToGlobal(const Point &offset) const;

        Region ToLocal(const Point &offset) const;

        bool Test(const Point &p) const;

        std::optional<Region> GetIntersection(const Region &other) const;

        void Clip(const Region &clipRegion);

        Region GetUnion(const Region &other) const;
    };

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
