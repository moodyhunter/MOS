#pragma once

#include "unit/template.hpp"

#include <chrono>
#include <functional>
#include <iostream>
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
// clang-format on

using UnitCreatorType = std::function<std::shared_ptr<Unit>(const std::string &, toml::table &)>;
using UnitInstantiator = std::function<std::shared_ptr<Unit>(const std::string &, std::shared_ptr<const Template> template_, const ArgumentMap &args)>;

struct Unit : public std::enable_shared_from_this<Unit>
{
    friend std::ostream &operator<<(std::ostream &, const Unit &);

  private:
    static constexpr auto inline toplevel = toplevel_t{ toplevel_t::_Construct::_Token };

  public:
    static void VerifyUnitArguments(const std::string &id, toml::table &table);
    static std::map<std::string, UnitCreatorType> &Creator(std::optional<std::pair<std::string, UnitCreatorType>> creator = std::nullopt);
    static std::shared_ptr<Unit> CreateNew(const std::string &id, const toml::table *data);

    static std::map<std::string, UnitInstantiator> &Instantiator(std::optional<std::pair<std::string, UnitInstantiator>> instantiator = std::nullopt);
    static std::shared_ptr<Unit> CreateFromTemplate(const std::string &id, std::shared_ptr<const Template> template_, const ArgumentMap &args);

  public:
    explicit Unit(const std::string &id, toml::table &table, std::shared_ptr<const Template> template_ = nullptr, const ArgumentMap &args = {});
    virtual ~Unit() = default;

    static std::string replace_all(std::string str, const std::string_view matcher, const std::string_view replacement)
    {
        for (size_t p = 0; (p = str.find(matcher, p)) != std::string::npos; p += replacement.length())
            str.replace(p, matcher.length(), replacement);
        return str;
    };

    const ArgumentMap arguments;

    const std::string id;
    const std::string description;

  public:
    virtual UnitType GetType() const = 0;
    virtual bool Start() = 0;
    virtual bool Stop() = 0;

  public:
    // clang-format off
    std::vector<std::string> GetDependencies() const { return dependsOn; }
    std::vector<std::string> GetPartOf() const { return partOf; }
    void AddDependency(const std::string &id) { dependsOn.push_back(id); }
    const UnitStatus &GetStatus() const { return status; }
    // clang-format on

    std::optional<std::string> GetFailReason() const
    {
        if (status.status == UnitStatus::UnitFailed)
            return status.message;
        return std::nullopt;
    }

  protected:
    std::string GetArg(toml::table &table, std::string_view key);
    std::vector<std::string> GetArrayArg(toml::table &table, std::string_view key);

  private:
    std::string GetArg(toml::table &table, std::string_view key, toplevel_t);
    std::vector<std::string> GetArrayArg(toml::table &table, std::string_view key, toplevel_t);

  private:
    virtual void onPrint(std::ostream &) const {};

    std::string ReplaceArgs(const std::string &str) const
    {
        // replace any $key with value in args
        std::string result = str;
        for (const auto &[key, value] : arguments)
            result = replace_all(result, "$" + key, value);
        return result;
    }

    std::vector<std::string> ReplaceArgs(const toml::array *array)
    {
        std::vector<std::string> result;
        for (const auto &e : *array)
        {
            if (!e.is_string())
            {
                std::cerr << "Invalid array element" << std::endl;
                continue;
            }
            result.push_back(ReplaceArgs(e.as_string()->get()));
        }
        return result;
    }

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
        return std::static_pointer_cast<Unit>(std::make_shared<TUnit>(id, table));
    }

    static constexpr auto Instantiator(const std::string &id, std::shared_ptr<const Template> template_, const ArgumentMap &args)
    {
        auto table = template_->table;
        const auto ptr = std::static_pointer_cast<Unit>(std::make_shared<TUnit>(id, table, template_, args));
        table.erase("template_args");
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
