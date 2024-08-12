// SPDX-License-Identifier: GPL-3.0-or-later

#include "parser.hpp"

#include <iostream>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

static std::vector<token> lex(const std::string &cmd)
{
    if (cmd.empty())
        return {};

    const char *data = cmd.data();
    const char *const end = data + cmd.size();
    std::vector<token> tokens;
    std::string curr;
    bool in_double_quotes = false;

    const auto flush_curr = [&]()
    {
        if (!curr.empty())
            tokens.push_back(token(curr)), curr.clear();
    };

    for (; data < end; data++)
    {
        switch (*data)
        {
            case '\\':
            {
                ++data;
                if (data == end)
                    throw std::runtime_error("unexpected end of command");
                if (!in_double_quotes && *data != '\n')
                    curr += *data;
                else
                {
                    switch (*data)
                    {
                        case '\n': break;
                        case '$': curr += '$'; break;
                        case '`': curr += '`'; break;
                        case '"': curr += '"'; break;
                        case '\\': curr += '\\'; break;
                        default: curr += '\\', curr += *data; break;
                    }
                }
                break;
            }

            case '\'':
            {
                if (in_double_quotes)
                    goto append_data;

                // read until next single quote
                while (++data < end)
                {
                    if (*data == '\'')
                        break;
                    curr += *data;
                }

                if (data == end)
                    throw std::runtime_error("Unterminated single quote");

                break;
            }

            case '"':
            {
                in_double_quotes = !in_double_quotes;
                break;
            }

            case '$':
            {
                // read until next token
                std::string var = "$";
                while (++data < end)
                {
                    if (*data == ' ' || *data == '\t' || *data == '\n' || *data == '\r' || *data == '\'' || *data == '"' || *data == '`')
                        break;
                    var += *data;
                }

                // match and replace the variable
                const auto regex = std::regex("(\\$[a-zA-Z0-9_]+)");

                std::smatch match;
                while (std::regex_search(var, match, regex))
                {
                    const auto varname = match[1].str();
                    const auto value = getenv(varname.c_str() + 1); // skip the $

                    if (value)
                        var = std::regex_replace(var, regex, value);
                    else
                        var = std::regex_replace(var, regex, "");
                }

                curr += var;
                break;
            }

            /// A space or tab ends a token.
            case ' ':
            case '\t':
            {
                if (in_double_quotes)
                    goto append_data;
                flush_curr();
                break;
            }

            case '&':
            {
                if (in_double_quotes)
                    goto append_data;
                flush_curr();
                tokens.push_back(token(BACKGROUND));
                break;
            }

            case '|':
            {
                if (in_double_quotes)
                    goto append_data;
                flush_curr();
                tokens.push_back(token(PIPE));
                break;
            }

            case '#':
            {
                if (in_double_quotes)
                    goto append_data;
                return tokens;
            }

            case '<':
            case '>':
            {
                if (in_double_quotes)
                    goto append_data;
                flush_curr();
                tokens.push_back(token(*data == '<' ? REDIRECT_IN : REDIRECT_OUT));
                break;
            }

            default:
            {
            append_data:
                curr += *data;
                break;
            }
        }
    }

    if (in_double_quotes)
        throw std::runtime_error("Unterminated double quote");
    if (!curr.empty())
        tokens.push_back({ curr });
    return tokens;
}

// implement a LL(1) parser for the shell grammar
static std::unique_ptr<ProgramSpec> parse_program(std::vector<token> &tokens)
{
    if (tokens.empty())
        return nullptr;

    auto program = std::make_unique<ProgramSpec>();
    program->argv.push_back(tokens[0].tstring);

    tokens.erase(tokens.begin());

    while (!tokens.empty())
    {
        const auto current_token = tokens[0];

        tokens.erase(tokens.begin());

        // implement a left-recursive descent lookahead 1 parser
        switch (current_token.type)
        {
            case REDIRECT_IN:
            case REDIRECT_OUT:
            case REDIRECT_APPEND:
            {
                if (tokens.empty())
                    throw std::runtime_error("Expected a filename after redirection");

                const auto filename = tokens[0].tstring;
                tokens.erase(tokens.begin());

                // 'program 2>&1' is not supported
                const auto fd = current_token.type == REDIRECT_IN ? 0 : 1;
                const auto mode = current_token.type == REDIRECT_IN ? BaseRedirection::IOMode::ReadOnly : BaseRedirection::IOMode::WriteOnly;
                const auto append = current_token.type == REDIRECT_APPEND;

                program->redirections[fd] = std::make_unique<FileRedirection>(filename, mode, append);
                break;
            }

            case PIPE:
            {
                std::cout << "PIPE isn't supported yet" << std::endl;
                break;
            }

            case BACKGROUND:
            {
                program->background = true;
                break;
            }

            case END:
            {
                return program;
            }

            default:
            {
                program->argv.push_back(current_token.tstring);
                break;
            }
        }
    }

    return program;
}

std::unique_ptr<ProgramSpec> parse_commandline(const std::string &command)
{
    try
    {
        auto tokens = lex(command);
        return parse_program(tokens);
    }
    catch (const std::exception &e)
    {
        std::cerr << "shlex: " << e.what() << std::endl;
        return {};
    }
}
