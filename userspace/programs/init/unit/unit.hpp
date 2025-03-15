#pragma once

#include <iostream>
#include <string>
#include <toml++/toml.hpp>
#include <vector>

enum class UnitStatus
{
    Stopped = 0,
    Starting = 1,
    Running = 2,
    Stopping = 3,
    Exited = 4,
    Failed = 5,
    Finished = 6,
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

struct Unit
{
    explicit Unit(const std::string &id) : id(id) {};
    virtual ~Unit() = default;

    const std::string id;

    std::string description;
    std::vector<std::string> depends_on;
    std::vector<std::string> part_of;

  public:
    virtual UnitType GetType() const = 0;

    UnitStatus GetStatus() const
    {
        return m_status;
    }

    std::optional<std::string> GetFailReason() const
    {
        return m_error;
    }

  public:
    virtual bool Start() = 0;
    virtual bool Stop() = 0;

  protected:
    virtual bool onLoad(const toml::table &) = 0;
    virtual void onPrint(std::ostream &) const {};

    static constexpr const auto VectorAppender = [](std::vector<std::string> &dest_container)
    {
        return [&](const toml::array &array) -> void
        {
            for (const auto &dep : array)
                dest_container.push_back(dep.as_string()->get());
        };
    };

  public:
    void load(const toml::table &data);

    friend std::ostream &operator<<(std::ostream &, const Unit &);

  protected:
    std::optional<std::string> m_error; ///< error message if failed to start

    void SetStatus(UnitStatus status)
    {
        m_status = status;
    }

  private:
    UnitStatus m_status = UnitStatus::Stopped;
};

std::ostream &operator<<(std::ostream &os, const Unit &unit);
