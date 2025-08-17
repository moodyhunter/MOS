// SPDX-License-Identifier: GPL-3.0-or-later

#include "common.hpp"

using namespace DisplayManager;

Delta Delta::operator+(const Delta &other) const
{
    return { x + other.x, y + other.y };
}

Delta Delta::operator-(const Delta &other) const
{
    return { x - other.x, y - other.y };
}

Point Point::ToGlobal(const Point &offset) const
{
    return { x + offset.x, y + offset.y };
}

Point Point::ToLocal(const Point &offset) const
{
    return { x - offset.x, y - offset.y };
}

Delta Point::operator-(const Point &other) const
{
    return { x - other.x, y - other.y };
}

Point Point::Clamped(const Size &size) const
{
    return {
        std::max(0, std::min(x, size.width - 1)),
        std::max(0, std::min(y, size.height - 1)),
    };
}

Point Point::Clamped(const Region &region) const
{
    return { std::max(region.origin.x, std::min(x, region.origin.x + region.size.width - 1)),
             std::max(region.origin.y, std::min(y, region.origin.y + region.size.height - 1)) };
}

Region Region::ToGlobal(const Point &offset) const
{
    return { origin.ToGlobal(offset), size };
}

Region Region::ToLocal(const Point &offset) const
{
    return { origin.ToLocal(offset), size };
}

std::optional<Region> Region::GetIntersection(const Region &other) const
{
    int x1 = std::max(origin.x, other.origin.x);
    int y1 = std::max(origin.y, other.origin.y);
    int x2 = std::min(origin.x + size.width, other.origin.x + other.size.width);
    int y2 = std::min(origin.y + size.height, other.origin.y + other.size.height);

    if (x1 < x2 && y1 < y2)
        return Region(Point(x1, y1), Size(x2 - x1, y2 - y1));
    return std::nullopt; ///< No intersection
}

void Region::Clip(const Region &clipRegion)
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

Region Region::GetUnion(const Region &other) const
{
    int x1 = std::min(origin.x, other.origin.x);
    int y1 = std::min(origin.y, other.origin.y);
    int x2 = std::max(origin.x + size.width, other.origin.x + other.size.width);
    int y2 = std::max(origin.y + size.height, other.origin.y + other.size.height);

    return Region(Point(x1, y1), Size(x2 - x1, y2 - y1));
}

bool Region::Test(const Point &p) const
{
    return p.x >= origin.x && p.x < (origin.x + size.width) && p.y >= origin.y && p.y < (origin.y + size.height);
}
