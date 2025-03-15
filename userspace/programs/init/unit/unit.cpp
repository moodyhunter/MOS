// SPDX-License-Identifier: GPL-3.0-or-later
#include "unit/unit.hpp"

std::ostream &operator<<(std::ostream &os, const Unit &unit)
{
    os << unit.description << " (" << unit.id << ")" << std::endl;
    os << "  depends_on: ";
    for (const auto &dep : unit.depends_on)
        os << dep << " ";
    if (unit.depends_on.empty())
        os << "(none)";
    os << std::endl;
    os << "  part_of: ";
    for (const auto &part : unit.part_of)
        os << part << " ";
    if (unit.part_of.empty())
        os << "(none)";
    os << std::endl;
    unit.onPrint(os);
    return os;
}

void Unit::load(const toml::table &data)
{
    this->description = data["description"].value_or("unknown");
    data["depends_on"].visit(VectorAppender(this->depends_on));
    data["part_of"].visit(VectorAppender(this->part_of));
    onLoad(data);
}
