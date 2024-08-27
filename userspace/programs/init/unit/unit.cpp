// SPDX-License-Identifier: GPL-3.0-or-later
#include "unit/unit.hpp"

bool Unit::start()
{
    if (m_status == UnitStatus::STOPPED || m_status == UnitStatus::FAILED)
    {
        m_status = UnitStatus::STARTING;
        if (do_start())
            m_status = UnitStatus::RUNNING;
        else
            m_status = UnitStatus::FAILED;
    }

    return m_status == UnitStatus::RUNNING;
}

void Unit::stop()
{
    if (m_status == UnitStatus::RUNNING || m_status == UnitStatus::FAILED)
    {
        m_status = UnitStatus::STOPPING;
        if (do_stop())
            m_status = UnitStatus::STOPPED;
        else
            m_status = UnitStatus::FAILED;
    }
}

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
    unit.do_print(os);
    return os;
}
void Unit::load(const toml::table &data)
{
    this->type = data["type"].value_or("unknown");
    this->description = data["description"].value_or("unknown");
    data["depends_on"].visit(array_append_visitor(this->depends_on));
    data["part_of"].visit(array_append_visitor(this->part_of));
    do_load(data);
}
