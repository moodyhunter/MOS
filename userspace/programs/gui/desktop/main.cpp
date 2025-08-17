// SPDX-License-Identifier: GPL-3.0-or-later

#include "proto/graphics-dm.pb.h"
#include "proto/graphics-dm.service.h"
#include "proto/graphics.pb.h"

#include <iostream>
#include <ostream>
#include <unistd.h>

WindowManagerStub wm{ "mos.window-manager" };

struct Size
{
    uint32_t width;
    uint32_t height;

    uint32_t GetPixels()
    {
        return width * height;
    }
};

static Size displaySize;

pb_bytes_array_t *windowContent;

int main(int argc, char **argv)
{
    const QueryDisplayInfoRequest request{ .display_name = (char *) "default" };
    QueryDisplayInfoResponse response{};
    wm.query_display_info(&request, &response);

    std::cout << "Display Info:" << std::endl;
    std::cout << "  Resolution: " << response.size.width << "x" << response.size.height << std::endl;

    displaySize.width = response.size.width;
    displaySize.height = response.size.height;

    windowContent = static_cast<pb_bytes_array_t *>(malloc(PB_BYTES_ARRAY_T_ALLOCSIZE(displaySize.GetPixels() * 4)));
    windowContent->size = displaySize.GetPixels() * 4;

    // set to white
    memset(windowContent->bytes, 0xbb, windowContent->size);

    // create a taskbar window
    CreateWindowRequest createRequest{
        .title = (char *) "Taskbar",
        .size = { .width = displaySize.width, .height = 40 },
        .specialType = SpecialWindowType_Desktop,
    };
    CreateWindowResponse createResponse{};
    wm.create_window(&createRequest, &createResponse);

    const auto wId = createResponse.window_id;
    std::cout << "Taskbar window id: " << wId << std::endl;

    MoveWindowRequest moveRequest{ .window_id = wId, .x = 0, .y = displaySize.height - 40 };
    MoveWindowResponse moveResponse;
    wm.move_window(&moveRequest, &moveResponse);

    while (true)
    {
        HandleEventRequest req{ .window_id = wId };
        HandleEventResponse resp;
        wm.handle_event(&req, &resp);

        const auto x = resp.event_data.event_type.mouse_move.position.x;
        const auto y = resp.event_data.event_type.mouse_move.position.y;

        std::cout << resp.event_data.event_type.mouse_move.position.x << ", " << resp.event_data.event_type.mouse_move.position.y << std::endl;

        char buf[100];

        pb_bytes_array_t *cont = (pb_bytes_array_t *) buf;
        cont->size = 4;
        cont->bytes[0] = 0x00;
        cont->bytes[1] = 0x00;
        cont->bytes[2] = 0xff;
        cont->bytes[3] = 0xff;

        UpdateWindowContentRequest updateReq{
            .window_id = wId,
            .region = graphics_Rectangle{ .x = x, .y = y, .w = 1, .h = 1 },
            .content = cont,
        };

        UpdateWindowContentResponse updateResp;
        wm.update_window_content(&updateReq, &updateResp);
    }
}
