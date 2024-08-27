#pragma once

#include <iostream>
#include <string>
#include <toml++/toml.hpp>
#include <vector>

enum class UnitStatus
{
    STOPPED = 0,
    STARTING = 1,
    RUNNING = 2,
    STOPPING = 3,
    EXITED = 4,
    FAILED = 5,
};

struct Unit
{
    explicit Unit(const std::string &id) : id(id) {};
    virtual ~Unit() = default;

    const std::string id;

    std::string type;
    std::string description;
    std::vector<std::string> depends_on;
    std::vector<std::string> part_of;

  public:
    UnitStatus status() const
    {
        return m_status;
    }

    std::string error_reason() const
    {
        return m_error;
    }

  public:
    bool start();
    void stop();

  protected:
    virtual bool do_start() = 0;
    virtual bool do_stop() = 0;
    virtual bool do_load(const toml::table &) = 0;
    virtual void do_print(std::ostream &) const {};

    static constexpr const auto array_append_visitor = [](std::vector<std::string> &dest_container)
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
    std::string m_error; ///< error message if failed to start

  private:
    UnitStatus m_status = UnitStatus::STOPPED;
};

std::ostream &operator<<(std::ostream &os, const Unit &unit);
