// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

struct BaseRedirection
{
    enum IOMode
    {
        ReadOnly = 1,
        WriteOnly = 2,
        ReadWrite = 3
    };

    IOMode mode = ReadOnly;
    bool append = false;

    virtual ~BaseRedirection() = default;
    virtual bool do_redirect(int fd) = 0;
};

struct FDRedirection : public BaseRedirection
{
    FDRedirection(int target_fd, IOMode mode) : target_fd(target_fd)
    {
        this->mode = mode;
    }

    bool do_redirect(int fd) override;
    int target_fd = -1;
};

struct FileRedirection : public BaseRedirection
{
    FileRedirection(const std::filesystem::path &file, IOMode mode, bool append) : file(file)
    {
        this->mode = mode;
        this->append = append;
    }

    bool do_redirect(int fd) override;
    std::filesystem::path file;
};

enum token_type
{
    TEXT,            // normal text
    REDIRECT_IN,     // <
    REDIRECT_OUT,    // >
    REDIRECT_APPEND, // >>
    PIPE,            // |
    BACKGROUND,      // &
    END              // end of command
};

struct token
{
    token_type type;
    std::string tstring;
    token(token_type type) : type(type), tstring() {};
    token(const std::string &tstring) : type(TEXT), tstring(tstring) {};

    friend std::ostream &operator<<(std::ostream &os, const token &t)
    {
        switch (t.type)
        {
            case TEXT: return os << "TEXT(" << t.tstring << ")";
            case REDIRECT_IN: os << "REDIRECT_IN"; break;
            case REDIRECT_OUT: os << "REDIRECT_OUT"; break;
            case REDIRECT_APPEND: os << "REDIRECT_APPEND"; break;
            case PIPE: os << "PIPE"; break;
            case BACKGROUND: os << "BACKGROUND"; break;
            case END: break;
        }
        return os;
    }
};

struct ProgramSpec
{
    std::vector<std::string> argv;
    std::map<int, std::unique_ptr<BaseRedirection>> redirections;
    bool background = false; // only valid for the last program in the pipeline

    friend std::ostream &operator<<(std::ostream &os, const ProgramSpec &program)
    {
        for (const auto &arg : program.argv)
            os << arg << ' ';
        os << std::endl;

        return os;
    }
};

std::unique_ptr<ProgramSpec> parse_commandline(const std::string &command);
