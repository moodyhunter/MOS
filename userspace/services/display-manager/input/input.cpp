// SPDX-License-Identifier: GPL-3.0-or-later

#include "input/input.hpp"

#include "libsm.h"
#include "mos/syscall/usermode.h"
#include "proto/graphics-gpu.pb.h"
#include "proto/graphics-gpu.service.h"

#include <librpc/rpc.h>
#include <mos/mos_global.h>
#include <stdio.h>
#include <thread>
#include <unistd.h>
#include <vector>

constexpr auto INPUTD_MODULE_PATH = "/initrd/modules/inputd.ko";

static s16 cursor_x = 0;
static s16 cursor_y = 0;

static void process_keyboard_event(int scancode)
{
    // Process keyboard event
    printf("Keyboard event: scancode %d\n", scancode);
}

static void process_mouse_event(const u8 data[3])
{
    static GraphicsManagerStub graphics_manager{ "gpu.virtio" };
    static int display_width = 800;  // Default display width
    static int display_height = 600; // Default display height
    if (once())
    {
        // Initialize cursor position
        cursor_x = 0;
        cursor_y = 0;
        printf("Mouse event handler initialized.\n");

        QueryDisplayInfoRequest request{ .display_name = (char *) "default_display" };
        QueryDisplayInfoResponse response;
        if (const auto ret = graphics_manager.query_display_info(&request, &response); ret != RPC_RESULT_OK)
        {
            fprintf(stderr, "Failed to call query_display_info on graphics manager: %d\n", ret);
            return;
        }

        display_width = response.width;
        display_height = response.height;
        printf("Display resolution: %dx%d\n", display_width, display_height);
    }

    bool left_button = data[0] & 0x01;   // Left button pressed
    bool right_button = data[0] & 0x02;  // Right button pressed
    bool middle_button = data[0] & 0x04; // Middle button pressed

    bool x_overflow = (data[0] & 0x40) != 0; // X overflow
    bool y_overflow = (data[0] & 0x80) != 0; // Y overflow

    const auto state = data[0];
    const auto x_movement = data[1] - ((state << 4) & 0x100);
    const auto y_movement = data[2] - ((state << 3) & 0x100);

    printf("Mouse event: Left: %d, Right: %d, Middle: %d, X: %d, Y: %d, X Overflow: %d, Y Overflow: %d\n", left_button, right_button, middle_button, x_movement,
           y_movement, x_overflow, y_overflow);

    // Update cursor position
    cursor_x += x_movement;
    cursor_y += -y_movement;
    if (cursor_x < 0)
        cursor_x = 0;
    if (cursor_y < 0)
        cursor_y = 0;

    if (cursor_x >= display_width)
        cursor_x = display_width - 1;
    if (cursor_y >= display_height)
        cursor_y = display_height - 1;

    const auto color = left_button ? 0xff0000ff : right_button ? 0xff00ff00 : middle_button ? 0xffff0000 : 0xffffffff;

    // mouse icon
    std::vector<u32> cursor_image(16 * 16, color);
    // make a border
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            if (x == 0 || x == 15 || y == 0 || y == 15)
                cursor_image[y * 16 + x] = 0xff000000; // Black border

    // Move cursor with the image
    PostBufferRequest post_buffer_request;
    post_buffer_request.display_name = (char *) "default_display";
    post_buffer_request.region.x = cursor_x;
    post_buffer_request.region.y = cursor_y;
    post_buffer_request.region.w = 16; // Width of the cursor image
    post_buffer_request.region.h = 16; // Height of the cursor image
    post_buffer_request.buffer_data = static_cast<pb_bytes_array_t *>(malloc(PB_BYTES_ARRAY_T_ALLOCSIZE(cursor_image.size() * sizeof(u32))));
    post_buffer_request.buffer_data->size = cursor_image.size() * sizeof(u32);
    memcpy(post_buffer_request.buffer_data->bytes, cursor_image.data(), post_buffer_request.buffer_data->size);

    PostBufferResponse post_buffer_response;
    if (graphics_manager.post_buffer(&post_buffer_request, &post_buffer_response) != RPC_RESULT_OK)
    {
        fprintf(stderr, "Failed to post cursor buffer: %s\n", post_buffer_response.result.error ? post_buffer_response.result.error : "Unknown error");
        free(post_buffer_request.buffer_data);
        return;
    }
}

static bool load_inputd_kmod()
{
    if (syscall_kmod_load(INPUTD_MODULE_PATH))
    {
        puts("Failed to load inputd kernel module");
        return false;
    }
    return true;
}

static void inputd_worker(fd_t fd)
{
    while (true)
    {
        int event_type;
        ssize_t bytes_read = read(fd, &event_type, sizeof(event_type));
        if (bytes_read < 0)
        {
            perror("Failed to read from inputd");
            ReportServiceState(UnitStatus::Failed, "inputd read failed");
            close(fd);
            return;
        }

        if (bytes_read == 0)
        {
            puts("No data available from inputd");
            continue; // No data, continue to the next iteration
        }

        if (event_type == 1) // Keyboard event
        {
            int scancode;
            bytes_read = read(fd, &scancode, sizeof(scancode));
            if (bytes_read < 0)
            {
                perror("Failed to read keyboard event");
                continue;
            }
            process_keyboard_event(scancode);
        }
        else if (event_type == 2) // Mouse event
        {
            u8 mouse_data[3];
            bytes_read = read(fd, mouse_data, sizeof(mouse_data));
            if (bytes_read < 0)
            {
                perror("Failed to read mouse event");
                continue;
            }
            process_mouse_event(mouse_data);
        }
        else
        {
            puts("Unknown event type received");
        }
    }
}

bool DisplayManager::initialise_input()
{
    load_inputd_kmod();
    fd_t fd = syscall_kmod_call("inputd", "subscribe", NULL, 0);
    if (fd < 0)
    {
        puts("Failed to subscribe to inputd");
        ReportServiceState(UnitStatus::Failed, "inputd subscription failed");
        return -1;
    }

    puts("Subscribed to inputd successfully");
    syscall_kmod_call("inputd", "enable", NULL, 0);
    std::thread worker_thread(inputd_worker, fd);
    worker_thread.detach();
    return true;
}
