// SPDX-License-Identifier: GPL-3.0-or-later

#include "libsm.h"
#include "mos/syscall/usermode.h"

#include <stdio.h>
#include <unistd.h>

constexpr auto INPUTD_MODULE_PATH = "/initrd/modules/inputd.ko";

static void process_keyboard_event(int scancode)
{
    // Process keyboard event
    printf("Keyboard event: scancode %d\n", scancode);
}

static void process_mouse_event(const u8 data[3])
{
    bool left_button = data[0] & 0x01;    // Left button pressed
    bool right_button = data[0] & 0x02;   // Right button pressed
    bool middle_button = data[0] & 0x04;  // Middle button pressed
    int8_t x_movement = (int8_t) data[1]; // X movement
    int8_t y_movement = (int8_t) data[2]; // Y movement

    bool x_overflow = (data[0] & 0x40) != 0; // X overflow
    bool y_overflow = (data[0] & 0x80) != 0; // Y overflow

    printf("Mouse event: Left: %d, Right: %d, Middle: %d, X: %d, Y: %d, X Overflow: %d, Y Overflow: %d\n", left_button, right_button, middle_button, x_movement,
           y_movement, x_overflow, y_overflow);
}

int main(int, char **)
{
    if (syscall_kmod_load(INPUTD_MODULE_PATH))
    {
        puts("Failed to load syslogd kernel module");
        ReportServiceState(UnitStatus::Failed, "syslogd kernel module load failed");
        return -1;
    }

    puts("Input device driver loaded successfully");
    ReportServiceState(UnitStatus::Started, "inputd kernel module loaded successfully");

    fd_t fd = syscall_kmod_call("inputd", "subscribe", NULL, 0);
    if (fd < 0)
    {
        puts("Failed to subscribe to inputd");
        ReportServiceState(UnitStatus::Failed, "inputd subscription failed");
        return -1;
    }

    puts("Subscribed to inputd successfully");
    syscall_kmod_call("inputd", "enable", NULL, 0);
    while (true)
    {
        int event_type;
        ssize_t bytes_read = read(fd, &event_type, sizeof(event_type));
        if (bytes_read < 0)
        {
            perror("Failed to read from inputd");
            ReportServiceState(UnitStatus::Failed, "inputd read failed");
            close(fd);
            return -1;
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
