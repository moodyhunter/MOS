#pragma once

#include "units/template.hpp"

#include <chrono>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <toml++/toml.hpp>
#include <vector>

struct UnitStatus
{
    enum MajorStatus
    {
        UnitStopped,
        UnitStarting,
        UnitStarted,
        UnitFailed,
        UnitStopping,
    };

    bool active = false;
    MajorStatus status = UnitStarting;
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
    std::string message;

    void Inactive()
    {
        active = false, status = UnitStopped, message.clear();
        UpdateTimestamp();
    }

    void Starting(const std::string &msg = "starting...")
    {
        active = true, status = UnitStarting, message = msg;
        UpdateTimestamp();
    }

    void Started(const std::string &msg = "success")
    {
        active = true, status = UnitStarted, message = msg;
        UpdateTimestamp();
    }

    void Failed(const std::string &msg = "failed")
    {
        active = true, status = UnitFailed, message = msg;
        UpdateTimestamp();
    }

    void Stopping(const std::string &msg = "stopping...")
    {
        active = true, status = UnitStopping, message = msg;
        UpdateTimestamp();
    }

  private:
    void UpdateTimestamp()
    {
        timestamp = std::chrono::system_clock::now();
    }
};

enum class UnitType
{
    Inherited = -1,

    Service, // 0
    Target,  // 1
    Path,    // 2
    Mount,   // 3
    Symlink, // 4
    Device,  // 5
    Timer,   // 6
};

// clang-format off
struct toplevel_t
{
    enum class _Construct { _Token };
    explicit consteval toplevel_t(_Construct) noexcept {};
};
struct custom_arg_t
{
    enum class _Construct { _Token };
    explicit consteval custom_arg_t(_Construct) noexcept {};
};
// clang-format on

struct IUnit : public std::enable_shared_from_this<IUnit>
{
    const std::string id;

    explicit IUnit(const std::string &id) : id(id) {};
    virtual ~IUnit() = default;

    std::string GetBaseId() const
    {
        std::string ret = id;
        if (const auto it = ret.find('@'); it != std::string::npos)
            ret = id.substr(0, it);

        if (ret.ends_with(TEMPLATE_SUFFIX))
            return ret.substr(0, ret.size() - TEMPLATE_SUFFIX.size());

        return ret;
    }

    virtual UnitType GetType() const = 0;
    virtual bool Start() = 0;
    virtual bool Stop() = 0;
    virtual std::string GetDescription() const = 0;
    virtual const UnitStatus &GetStatus() const = 0;
    virtual std::vector<std::string> GetDependencies() const = 0;
    virtual std::vector<std::string> GetPartOf() const = 0;
    virtual std::optional<std::string> GetFailReason() const = 0;
    virtual void AddDependency(const std::string &depName) = 0;
};

using UnitCreatorType = std::function<std::shared_ptr<IUnit>(const std::string &, toml::table &)>;
using UnitInstantiator = std::function<std::shared_ptr<IUnit>(const std::string &, std::shared_ptr<const Template> template_, const ArgumentMap &args)>;

struct Unit : public IUnit
{
    friend std::ostream &operator<<(std::ostream &, const Unit &);

  private:
    static constexpr auto inline toplevel = toplevel_t{ toplevel_t::_Construct::_Token };
    static constexpr auto inline custom_arg = custom_arg_t{ custom_arg_t::_Construct::_Token };

  public:
    static const std::map<std::string, UnitCreatorType> &Creator(std::optional<std::pair<std::string, UnitCreatorType>> creator = std::nullopt);
    static const std::map<std::string, UnitInstantiator> &Instantiator(std::optional<std::pair<std::string, UnitInstantiator>> instantiator = std::nullopt);

    static std::shared_ptr<IUnit> Create(const std::string &id, const toml::table &data);
    static std::shared_ptr<IUnit> Instantiate(const std::string &id, std::shared_ptr<const Template> template_, const ArgumentMap &args);

    static void VerifyUnitArguments(const std::string &id, const toml::table &table);

    static std::string replace_all(std::string str, const std::string_view matcher, const std::string_view replacement)
    {
        size_t pos = 0;
        while ((pos = str.find(matcher, pos)) != std::string::npos)
        {
            str.replace(pos, matcher.length(), replacement);
            pos += replacement.length();
        }
        return str;
    }

  public:
    const ArgumentMap arguments;
    const std::string description;

  public:
    explicit Unit(const std::string &id, toml::table &table, std::shared_ptr<const Template> template_ = nullptr, const ArgumentMap &args = {});
    virtual ~Unit() override = default;

  public:
    virtual UnitType GetType() const override = 0;
    virtual bool Start() override = 0;
    virtual bool Stop() override = 0;

  public:
    // clang-format off
    std::string GetDescription() const override { return description; }
    std::vector<std::string> GetDependencies() const override { return dependsOn; }
    std::vector<std::string> GetPartOf() const override { return partOf; }
    void AddDependency(const std::string &id) override { dependsOn.push_back(id); }
    const UnitStatus &GetStatus() const override { return status; }
    // clang-format on

    std::optional<std::string> GetFailReason() const override
    {
        if (status.status == UnitStatus::UnitFailed)
            return status.message;
        return std::nullopt;
    }

  protected:
    std::string PopArg(toml::table &table, std::string_view key);
    std::vector<std::string> GetArrayArg(toml::table &table, std::string_view key);

  private:
    std::string PopArg(toml::table &table, std::string_view key, toplevel_t);
    std::vector<std::string> GetArrayArg(toml::table &table, std::string_view key, toplevel_t);

  private:
    virtual void onPrint(std::ostream &) const {};
    std::string ReplaceArgs(const std::string &str) const;
    std::vector<std::string> ReplaceArgs(const toml::array *array);

  protected:
    UnitStatus status;

  private:
    std::vector<std::string> dependsOn;
    std::vector<std::string> partOf;
    const std::shared_ptr<const Template> template_;
};

template<typename TUnit>
struct UnitRegisterer
{
    static constexpr auto Creator(const std::string &id, toml::table &table)
    {
        return std::static_pointer_cast<IUnit>(std::make_shared<TUnit>(id, table));
    }

    static constexpr auto Instantiator(const std::string &id, std::shared_ptr<const Template> template_, const ArgumentMap &args)
    {
        auto table = template_->table;
        const auto ptr = std::static_pointer_cast<IUnit>(std::make_shared<TUnit>(id, table, template_, args));
        table.erase("template_params");
        Unit::VerifyUnitArguments(id, table);
        return ptr;
    }

    explicit UnitRegisterer(const std::string &type)
    {
        Unit::Creator(std::make_pair(type, Creator));
        Unit::Instantiator(std::make_pair(type, Instantiator));
    }
};

#define RegisterUnit(name, Name) static __attribute__((__used__)) UnitRegisterer<Name> __register_##name(#name)
