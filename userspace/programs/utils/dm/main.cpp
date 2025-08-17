// SPDX-License-Identifier: GPL-3.0-or-later

#include "proto/graphics-dm.pb.h"
#include "proto/graphics-dm.service.h"

#include <cassert>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <vector>
WindowManagerStub windowManagerStub("mos.window-manager");

struct BMPImage
{
    BMPImage(int width, int height) : w(width), h(height), rgb(w * h * 3)
    {
    }
    uint8_t &r(int x, int y)
    {
        return rgb[(x + y * w) * 3 + 2];
    }
    uint8_t &g(int x, int y)
    {
        return rgb[(x + y * w) * 3 + 1];
    }
    uint8_t &b(int x, int y)
    {
        return rgb[(x + y * w) * 3 + 0];
    }

    int w, h;
    std::vector<uint8_t> rgb;
};

template<class Stream>
Stream &operator<<(Stream &out, BMPImage const &img)
{
    uint32_t w = img.w, h = img.h;
    uint32_t pad = w * -3 & 3;
    uint32_t total = 54 + 3 * w * h + pad * h;
    uint32_t head[13] = { total, 0, 54, 40, w, h, (24 << 16) | 1 };
    char const *rgb = (char const *) img.rgb.data();

    out.write("BM", 2);
    out.write((char *) head, 52);
    for (uint32_t i = 0; i < h; i++)
    {
        out.write(rgb + (3 * w * i), 3 * w);
        out.write((char *) &pad, pad);
    }
    return out;
}

void DoListWindows(const std::vector<std::string> &)
{
    GetWindowListRequest request{};
    GetWindowListResponse response;

    if (windowManagerStub.get_window_list(&request, &response) != RPC_RESULT_OK)
    {
        std::cerr << "Failed to get window list." << std::endl;
        return;
    }

    std::cout << "Window List:" << std::endl;
    for (size_t i = 0; i < response.windows_count; ++i)
    {
        const auto *window = &response.windows[i];
        std::cout << "Window ID: " << window->window_id << ", Title: " << window->title << ", Position: (" << window->bounds.x << ", " << window->bounds.y << ")"
                  << ", Size: (" << window->bounds.w << "x" << window->bounds.h << ")" << std::endl;
    }
}

void DoScreenShot(const std::vector<std::string> &args)
{
    if (args.size() < 1)
    {
        std::cerr << "Usage: screenshot <filename>" << std::endl;
        return;
    }

    const std::string &filename = args[0];
    std::cout << "Taking screenshot and saving to " << filename << "..." << std::endl;

    ScreenshotRequest request{ .window_id = 0 }; // 0 for the entire screen
    ScreenshotResponse response;

    if (windowManagerStub.do_screenshot(&request, &response) != RPC_RESULT_OK)
    {
        std::cerr << "Failed to take screenshot." << std::endl;
        return;
    }

    if (!response.result.success)
    {
        std::cerr << "Screenshot failed: " << (response.result.error ? response.result.error : "Unknown error") << std::endl;
        return;
    }

    std::cout << "Screenshot taken successfully. Size: " << response.size.width << "x" << response.size.height << std::endl;

    static BMPImage img(response.size.width, response.size.height);

    for (uint32_t x = 0; x < response.size.width; x++)
    {
        for (uint32_t y = 0; y < response.size.height; y++)
        {
            const uint32_t argb = ((uint32_t *) response.image->bytes)[x + (response.size.height - y) * response.size.width];
            img.r(x, y) = argb >> 16;         // Red channel
            img.g(x, y) = (argb >> 8) & 0xFF; // Green channel
            img.b(x, y) = argb & 0xFF;        // Blue channel
        }
    }

    std::ofstream(filename) << img << std::flush;
    std::cout << "Screenshot saved successfully." << std::endl;
}

using Command = std::function<void(const std::vector<std::string> &args)>;

const std::map<std::string, Command> commands = {
    { "list", DoListWindows },
    { "screenshot", DoScreenShot },
};

int main(int argc, char **argv)
{
    std::cout << "Display Manager Debugger for MOS" << std::endl;

    if (argc < 2)
    {
        DoListWindows({});
        return 0;
    }

    const std::string commandName = argv[1];
    const std::vector<std::string> args(argv + 2, argv + argc);

    const auto commandIt = commands.find(commandName);
    if (commandIt == commands.end())
    {
        std::cerr << "Unknown command: " << commandName << std::endl;
        std::cerr << "Available commands: ";
        for (const auto &cmd : commands)
        {
            std::cout << cmd.first << " ";
        }
        std::cout << std::endl;
        return 1;
    }

    const auto &command = commandIt->second;
    if (!command)
    {
        std::cerr << "Command '" << commandName << "' is not implemented." << std::endl;
        return 1;
    }

    // Execute the command
    command(args);

    return 0;
}
