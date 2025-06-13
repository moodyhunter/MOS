#include "units/inherited.hpp"

#include "unit.hpp"

#include <memory>
#include <string>

InheritedUnit::InheritedUnit(const std::string &id, std::shared_ptr<IUnit> baseUnit)
    : IUnit(id),                      //
      childUnit_(std::move(baseUnit)) //
{
}

std::optional<std::string> InheritedUnit::GetFailReason() const
{
    return childUnit_->GetFailReason();
}

std::vector<std::string> InheritedUnit::GetPartOf() const
{
    return childUnit_->GetPartOf();
}

std::vector<std::string> InheritedUnit::GetDependencies() const
{
    return childUnit_->GetDependencies();
}

const UnitStatus &InheritedUnit::GetStatus() const
{
    return childUnit_->GetStatus();
}

bool InheritedUnit::Stop()
{
    return childUnit_->Stop();
}

bool InheritedUnit::Start()
{
    return childUnit_->Start();
}

UnitType InheritedUnit::GetType() const
{
    return UnitType::Inherited;
}

std::string InheritedUnit::GetChildId() const
{
    return childUnit_->id;
}

std::string InheritedUnit::GetDescription() const
{
    return childUnit_->GetDescription();
}

void InheritedUnit::AddDependency(const std::string &depName)
{
    childUnit_->AddDependency(depName);
}
