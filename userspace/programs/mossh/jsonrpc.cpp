// SPDX-License-Identifier: GPL-3.0-or-later

#include "LaunchContext.hpp"
#include "mossh.hpp"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>
#include <unistd.h>

using namespace std::string_view_literals;

#define IsRequest(_type)    (packet.type == rpc::_type::rpc_type + ".request"s)
#define IsResponse(_type)   (packet.type == rpc::_type::rpc_type + ".response"s)
#define ResponseType(_type) (rpc::_type::rpc_type + ".response"s)

namespace rpc
{
    struct Packet
    {
        std::string type;
        nlohmann::json object;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Packet, type, object)
    };

    namespace RunCommand
    {
        constexpr static const auto rpc_type = "run-command"s;

        struct RedirectionEntry
        {
            bool read = false, write = false, append = false;
            std::string type;   // "file" or "fd"
            std::string target; // a file path or another fd
            NLOHMANN_DEFINE_TYPE_INTRUSIVE(RedirectionEntry, type, target, read, write, append)
        };

        struct Request
        {
            std::string command;
            std::vector<std::string> argv;
            std::map<std::string, RedirectionEntry> redirections; // defect: should be int instead of string
            NLOHMANN_DEFINE_TYPE_INTRUSIVE(Request, command, argv, redirections)
        };

        struct Response
        {
            int returncode;
            NLOHMANN_DEFINE_TYPE_INTRUSIVE(Response, returncode)
        };
    } // namespace RunCommand

    namespace Shutdown
    {
        constexpr static const auto rpc_type = "shutdown"s;

        struct Request
        {
            std::string stub;
            NLOHMANN_DEFINE_TYPE_INTRUSIVE(Request, stub)
        };

        struct Response
        {
            std::string stub;
            NLOHMANN_DEFINE_TYPE_INTRUSIVE(Response, stub)
        };
    } // namespace Shutdown

    namespace ReadFile
    {
        constexpr static const auto rpc_type = "read-file"s;

        struct Request
        {
            std::string path;
            NLOHMANN_DEFINE_TYPE_INTRUSIVE(Request, path)
        };

        struct Response
        {
            std::string content;
            NLOHMANN_DEFINE_TYPE_INTRUSIVE(Response, content)
        };
    } // namespace ReadFile

    namespace WriteFile
    {
        constexpr static const auto rpc_type = "write-file"s;

        struct Request
        {
            std::string path;
            std::string content;
            NLOHMANN_DEFINE_TYPE_INTRUSIVE(Request, path, content)
        };

        struct Response
        {
            std::string stub;
            NLOHMANN_DEFINE_TYPE_INTRUSIVE(Response, stub)
        };
    } // namespace WriteFile

} // namespace rpc

int do_jsonrpc()
{
    std::string input;

    while (true)
    {
        std::getline(std::cin, input);
        nlohmann::json j = nlohmann::json::parse(input);

        const auto packet = j.get<rpc::Packet>();

        rpc::Packet resp;

        if (IsRequest(RunCommand))
        {
            resp.type = ResponseType(RunCommand);
            const auto request = packet.object.get<const rpc::RunCommand::Request>();

            LaunchContext ctx{ request.argv };
            ctx.should_wait = true;

            for (const auto &[fd, redir] : request.redirections)
            {
                const auto fd_int = std::stoi(fd);
                if (redir.type == "file")
                    ctx.redirect(fd_int, std::make_unique<FileRedirection>(redir.target, BaseRedirection::IOMode::ReadWrite, redir.append));
                else if (redir.type == "fd")
                    ctx.redirect(fd_int, std::make_unique<FDRedirection>(std::stoi(redir.target), BaseRedirection::IOMode::ReadWrite));
                else
                    std::cerr << "Skipped unknown redirection type: " << redir.type << std::endl;
            }

            const auto result = ctx.start();
            if (!result)
                std::cerr << "Failed to start program: " << ctx.command() << std::endl;

            const rpc::RunCommand::Response response{ .returncode = ctx.exit_code };
            resp.object = response;
        }
        else if (IsRequest(Shutdown))
        {
            resp.type = ResponseType(Shutdown);
            const auto request = packet.object.get<const rpc::Shutdown::Request>();

            // run shutdown in a separate thread
            std::thread([]() { sleep(3), execute_line("shutdown"); }).detach();

            const rpc::Shutdown::Response response{
                .stub = request.stub,
            };

            resp.object = response;
        }
        else if (IsRequest(ReadFile))
        {
            resp.type = ResponseType(ReadFile);
            const auto request = packet.object.get<const rpc::ReadFile::Request>();

            const auto read_file = [](const std::string &path) -> std::string
            {
                std::ifstream file(path);
                if (!file.is_open())
                    return "";

                std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                return content;
            };

            const auto content = read_file(request.path);
            const rpc::ReadFile::Response response{ .content = content };

            resp.object = response;
        }
        else if (IsRequest(WriteFile))
        {
            resp.type = ResponseType(WriteFile);
            const auto request = packet.object.get<const rpc::WriteFile::Request>();

            const auto write_file = [](const std::string &path, const std::string &content)
            {
                std::ofstream file(path);
                if (!file.is_open())
                    return;

                file << content;
            };

            write_file(request.path, request.content);
            const rpc::WriteFile::Response response{ .stub = "" };

            resp.object = response;
        }
        else
        {
            std::cerr << "Unknown packet type: " << packet.type << std::endl;
        }

        std::cout << static_cast<nlohmann::json>(resp).dump() << std::endl;
    }

    return 0;
}
