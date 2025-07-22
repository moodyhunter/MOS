// SPDX-License-Identifier: GPL-3.0-or-later

#include "render/renderer.hpp"

#include "proto/graphics-gpu.pb.h"
#include "proto/graphics-gpu.service.h"
#include "utils/SubView.hpp"
#include "utils/common.hpp"
#include "windows/window-manager.hpp"

#include <iostream>
#include <librpc/rpc.h>
#include <memory>
#include <pb.h>
#include <ranges>

using namespace DisplayManager::Render;

static void FillBackgroundColor(pb_bytes_array_t *buffer, bool transparent = false)
{
    // Initialize the back buffer with a solid color #8affdb
    for (size_t i = 0; i < buffer->size / sizeof(uint32_t); ++i)
    {
        reinterpret_cast<uint32_t *>(buffer->bytes)[i] = transparent ? 0x00000000 : 0xff8affdb; // Transparent or solid color
    }
}

RendererClass::RendererClass()
{
}

RendererClass::~RendererClass()
{
    if (back_buffer)
    {
        free(back_buffer);
    }
}

bool RendererClass::Initialize()
{
    graphics_manager = std::make_unique<GraphicsManagerStub>("gpu.virtio");
    if (!graphics_manager)
    {
        std::cerr << "Failed to create GraphicsManagerStub." << std::endl;
        return false;
    }

    const QueryDisplayInfoRequest request{ .display_name = (char *) "default_display" };
    QueryDisplayInfoResponse response;

    if (graphics_manager->query_display_info(&request, &response) != RPC_RESULT_OK)
    {
        std::cerr << "Failed to query display info." << std::endl;
        return false;
    }

    std::cout << "Display Name: " << response.display_name << std::endl;
    std::cout << "Resolution: " << response.width << "x" << response.height << std::endl;

    display_info.width = response.width;
    display_info.height = response.height;

    // Allocate the back buffer
    const auto buffersize = response.width * response.height * sizeof(uint32_t);
    back_buffer = static_cast<pb_bytes_array_t *>(malloc(PB_BYTES_ARRAY_T_ALLOCSIZE(buffersize)));
    if (!back_buffer)
    {
        std::cerr << "Failed to allocate back buffer." << std::endl;
        return false;
    }

    back_buffer->size = buffersize;

    FillBackgroundColor(back_buffer);
    return true;
}

bool RendererClass::RenderFullScreen()
{
    return DamageGlobal(Region{ Point{ 0, 0 }, Size{ display_info.width, display_info.height } });
}

void RendererClass::SetCursorPosition(int, int)
{
}

bool RendererClass::DamageGlobal(const Region &region)
{
    const auto contentSize = region.size.width * region.size.height * sizeof(uint32_t);
    pb_bytes_array_t *content = static_cast<pb_bytes_array_t *>(malloc(PB_BYTES_ARRAY_T_ALLOCSIZE(contentSize)));
    if (!content)
    {
        std::cerr << "Failed to allocate content buffer." << std::endl;
        return false;
    }
    content->size = contentSize;

    FillBackgroundColor(content, false);

    static std::mutex mutex;
    std::lock_guard lock{ mutex };

    // from the bottom to the top
    for (const auto wId : std::ranges::reverse_view(Windows::ZOrder))
    {
        const auto window = Windows::WindowManager->GetWindow(wId);
        if (!window)
        {
            std::cerr << "Window with ID " << wId << " not found." << std::endl;
            continue;
        }

        const auto windowRegion = window->GetWindowRegion().GetIntersection(region);
        if (!windowRegion)
            continue;

        Utils::SubView<uint32_t> subView(content, region.size, Region{ windowRegion->origin - region.origin, windowRegion->size });

        if (!window->GetRegionContent(windowRegion->ToLocal(window->GetPosition()), subView))
        {
            std::cerr << "Failed to get content for window ID " << wId << " in region (" << windowRegion->origin.x << ", " << windowRegion->origin.y << ") Size: ("
                      << windowRegion->size.width << "x" << windowRegion->size.height << ")" << std::endl;
            continue;
        }
    }

    if (!DoPostBuffer(region, content))
    {
        std::cerr << "Failed to post buffer for region (" << region.origin.x << ", " << region.origin.y << ") Size: (" << region.size.width << "x" << region.size.height
                  << ")" << std::endl;
        free(content);
        return false;
    }

    free(content);
    return true;
}

bool RendererClass::DoPostBuffer(const Region &region, pb_bytes_array_t *content)
{
    static std::mutex mutex;
    std::lock_guard lock{ mutex };

    PostBufferRequest post_buffer_request;
    post_buffer_request.display_name = (char *) "default_display";
    post_buffer_request.region.x = region.origin.x;
    post_buffer_request.region.y = region.origin.y;
    post_buffer_request.region.w = region.size.width;
    post_buffer_request.region.h = region.size.height;
    post_buffer_request.buffer_data = content;

    PostBufferResponse post_buffer_response;
    if (graphics_manager->post_buffer(&post_buffer_request, &post_buffer_response) != RPC_RESULT_OK)
    {
        std::cerr << "Failed to post buffer: RPC error" << std::endl;
        return false;
    }

    if (!post_buffer_response.result.success)
    {
        std::cerr << "Failed to post buffer: " << post_buffer_response.result.error << std::endl;
        return false;
    }

    return true;
}
