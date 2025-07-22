// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/syscall/usermode.h"
#include "proto/graphics-dm.service.h"

#include <chrono>
#include <cmath>
#include <cstdlib>

static WindowManagerStub wm{ "mos.window-manager" };

#define CLOCK_SIZE   200
#define CLOCK_RADIUS (CLOCK_SIZE / 2)

void draw_circle(u32 *buffer, size_t width, size_t height, size_t center_x, size_t center_y, size_t radius, u32 color)
{
    for (size_t y = 0; y < height; ++y)
    {
        for (size_t x = 0; x < width; ++x)
        {
            if ((x - center_x) * (x - center_x) + (y - center_y) * (y - center_y) <= radius * radius)
            {
                buffer[y * width + x] = color;
            }
        }
    }
}

void draw_line(u32 *buffer, size_t width, size_t height, size_t x1, size_t y1, size_t x2, size_t y2, u32 color)
{
    int dx = x2 - x1;
    int dy = y2 - y1;
    int steps = std::max(abs(dx), abs(dy));
    float x_inc = static_cast<float>(dx) / steps;
    float y_inc = static_cast<float>(dy) / steps;

    float x = x1;
    float y = y1;
    for (int i = 0; i <= steps; ++i)
    {
        if (x >= 0 && x < width && y >= 0 && y < height)
        {
            buffer[static_cast<size_t>(y) * width + static_cast<size_t>(x)] = color;
        }
        x += x_inc;
        y += y_inc;
    }
}

int main(int, char **)
{
    CreateWindowRequest create_window_request;
    create_window_request.display_name = (char *) "Clock Window";
    create_window_request.width = CLOCK_SIZE;
    create_window_request.height = CLOCK_SIZE;
    CreateWindowResponse create_window_response;
    wm.create_window(&create_window_request, &create_window_response);

    printf("Created window with ID: %lu\n", create_window_response.window_id);

    auto buffer_data = static_cast<pb_bytes_array_t *>(malloc(PB_BYTES_ARRAY_T_ALLOCSIZE(CLOCK_SIZE * CLOCK_SIZE * sizeof(u32))));
    buffer_data->size = CLOCK_SIZE * CLOCK_SIZE * sizeof(u32);

    u32 *buffer = (u32 *) buffer_data->bytes;

    auto time = std::chrono::system_clock::now();
    while (true)
    {
        if (time == std::chrono::system_clock::now())
        {
            syscall_yield_cpu();
            continue; // Avoid busy-waiting
        }

        time = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(time);
        auto tm = *std::localtime(&time_t);

        // fill the buffer with a color pattern
        memset(buffer, 0x00000000, CLOCK_SIZE * CLOCK_SIZE * sizeof(u32));

        draw_circle(buffer, CLOCK_SIZE, CLOCK_SIZE, CLOCK_RADIUS, CLOCK_RADIUS, CLOCK_RADIUS, 0xff0000ff);
        draw_circle(buffer, CLOCK_SIZE, CLOCK_SIZE, CLOCK_RADIUS, CLOCK_RADIUS, CLOCK_RADIUS - 10, 0xffffffff);

        // draw hour hand
        size_t hour_x = CLOCK_RADIUS + static_cast<size_t>(50 * std::cos((tm.tm_hour % 12) * 30 * M_PI / 180 - M_PI / 2));
        size_t hour_y = CLOCK_RADIUS + static_cast<size_t>(50 * std::sin((tm.tm_hour % 12) * 30 * M_PI / 180 - M_PI / 2));
        draw_line(buffer, CLOCK_SIZE, CLOCK_SIZE, CLOCK_RADIUS, CLOCK_RADIUS, hour_x, hour_y, 0xff0000ff);

        // draw minute hand
        size_t minute_x = CLOCK_RADIUS + static_cast<size_t>(75 * std::cos((tm.tm_min * 6) * M_PI / 180 - M_PI / 2));
        size_t minute_y = CLOCK_RADIUS + static_cast<size_t>(75 * std::sin((tm.tm_min * 6) * M_PI / 180 - M_PI / 2));
        draw_line(buffer, CLOCK_SIZE, CLOCK_SIZE, CLOCK_RADIUS, CLOCK_RADIUS, minute_x, minute_y, 0xff00ff00);

        // draw second hand
        size_t second_x = CLOCK_RADIUS + static_cast<size_t>(90 * std::cos((tm.tm_sec * 6) * M_PI / 180 - M_PI / 2));
        size_t second_y = CLOCK_RADIUS + static_cast<size_t>(90 * std::sin((tm.tm_sec * 6) * M_PI / 180 - M_PI / 2));
        draw_line(buffer, CLOCK_SIZE, CLOCK_SIZE, CLOCK_RADIUS, CLOCK_RADIUS, second_x, second_y, 0xffff0000);

        UpdateWindowContentRequest update_request;
        update_request.window_id = create_window_response.window_id;
        update_request.region.x = 0;
        update_request.region.y = 0;
        update_request.region.w = CLOCK_SIZE;
        update_request.region.h = CLOCK_SIZE;
        update_request.content = buffer_data;
        UpdateWindowContentResponse update_response;
        wm.update_window_content(&update_request, &update_response);
    }
}
