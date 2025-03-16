// SPDX-License-Identifier: GPL-3.0-or-later

#include "unit/template.hpp"

#include "logging.hpp"
#include "unit/unit.hpp"

bool VerifyArguments(const std::vector<std::string> &params, const ArgumentMap &args)
{
    const auto &rhs = args;
    const auto b1 = std::all_of(params.begin(), params.end(), [&rhs](const auto &p) { return rhs.contains(p); });
    const auto b2 = std::all_of(rhs.begin(), rhs.end(), [&params](const auto &pair) { return std::find(params.begin(), params.end(), pair.first) != params.end(); });

    if (!b1)
        Debug << "Missing required arguments for unit instantiation." << std::endl;

    if (!b2)
        Debug << "Extraneous arguments for unit instantiation." << std::endl;

    return b1 && b2;
}

std::shared_ptr<Unit> Template::Instantiate(const ArgumentMap &args) const
{
    if (!VerifyArguments(parameters, args))
        return nullptr;

    return Unit::CreateFromTemplate(id, shared_from_this(), args);
}
