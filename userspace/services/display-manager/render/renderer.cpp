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

using namespace DisplayManager;
using namespace DisplayManager::Render;

static std::shared_ptr<DisplayManager::Windows::Window> cursorWindow = nullptr;
static std::shared_ptr<DisplayManager::Windows::Window> backgroundWindow = nullptr;

static const u32 cursorImage[] = {
    0xff000000, 0xff4f4f4f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0xff4f4f4f, 0xff000000, 0xff000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xff000000, 0xffb3b3b3, 0xff000000, 0xff000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xff000000, 0xffb3b3b3,
    0xffb3b3b3, 0xff000000, 0xff000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0xff000000, 0xffb3b3b3, 0xffb3b3b3, 0xffb3b3b3, 0xffb3b3b3, 0xff000000, 0xff000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xff000000, 0xffb3b3b3, 0xffb3b3b3, 0xffb3b3b3, 0xffb3b3b3, 0xffb3b3b3, 0xff000000, 0xff000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xff000000, 0xffb3b3b3, 0xffb3b3b3, 0xffb3b3b3, 0xffb3b3b3,
    0xffb3b3b3, 0xffb3b3b3, 0xffb3b3b3, 0xff000000, 0xff000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xff000000, 0xffb3b3b3,
    0xffb3b3b3, 0xffb3b3b3, 0xffb3b3b3, 0xffb3b3b3, 0xffb3b3b3, 0xffb3b3b3, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0xff000000, 0xffb3b3b3, 0xffb3b3b3, 0xffb3b3b3, 0xffb3b3b3, 0xffb3b3b3, 0xffb3b3b3, 0xff000000, 0xffffffff, 0xff000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xff000000, 0xffb3b3b3, 0xffb3b3b3, 0xffb3b3b3, 0xffb3b3b3, 0xffb3b3b3, 0xffb3b3b3, 0xff000000,
    0xff000000, 0xff000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xff000000, 0xffb3b3b3, 0xffb3b3b3, 0xffb3b3b3, 0xffb3b3b3,
    0xffb3b3b3, 0xff000000, 0xff000000, 0xffffffff, 0xff000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xff000000,
    0xffb3b3b3, 0xffb3b3b3, 0xffb3b3b3, 0xff000000, 0xff000000, 0xffffffff, 0xff000000, 0xff000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xffffffff, 0xffffffff, 0xff000000, 0xff000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xff000000, 0xffffffff, 0xff000000, 0xffffffff, 0xff000000, 0xff000000, 0xff000000,
    0xff000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000,
    0x00000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xff000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xff000000, 0xff000000, 0x00000000,
};

static void FillBackgroundColor(void *buffer, size_t bufSize, bool transparent = false)
{
    // Initialize the back buffer with a solid color #8affdb
    for (size_t i = 0; i < bufSize / sizeof(uint32_t); ++i)
    {
        reinterpret_cast<uint32_t *>(buffer)[i] = transparent ? 0x00000000 : 0xff8affdb; // Transparent or solid color
    }
}

RendererClass::RendererClass()
{
}

RendererClass::~RendererClass()
{
}

bool RendererClass::Initialize()
{
    graphics_manager = std::make_unique<GraphicsManagerStub>("gpu.virtio");
    if (!graphics_manager)
    {
        std::cerr << "Failed to create GraphicsManagerStub." << std::endl;
        return false;
    }

    const GpuQueryDisplayInfoRequest request{ .display_name = (char *) "default_display" };
    GpuQueryDisplayInfoResponse response;

    if (graphics_manager->query_display_info(&request, &response) != RPC_RESULT_OK)
    {
        std::cerr << "Failed to query display info." << std::endl;
        return false;
    }

    std::cout << "Display Name: " << response.display_name << std::endl;
    std::cout << "Resolution: " << response.size.width << "x" << response.size.height << std::endl;

    display_info.size = Size{ static_cast<int>(response.size.width), static_cast<int>(response.size.height) };

    cursorWindow = Windows::WindowManager->CreateWindow("mouse", { 0, 0 }, { 16, 16 }, Windows::WindowType::Cursor);
    cursorWindow->UpdateContent(cursorWindow->GetWindowRegion(), cursorImage, sizeof(cursorImage));

    backgroundWindow = Windows::WindowManager->CreateWindow("background", { 0, 0 }, display_info.size, Windows::WindowType::Background);
    FillBackgroundColor(backgroundWindow->GetBackingBuffer()->bytes, backgroundWindow->GetBackingBuffer()->size, false);
    return true;
}

bool RendererClass::RenderFullScreen()
{
    return DamageGlobal(Region{ Point{ 0, 0 }, display_info.size });
}

Point RendererClass::SetCursorPosition(Point position)
{
    const auto clamped = position.Clamped(display_info.size);
    Windows::WindowManager->MoveWindowTo(cursorWindow->windowId, clamped);
    return clamped;
}

extern void *renderBuffer;

bool RendererClass::DamageGlobal(const Region &in)
{
    const auto region = in.GetIntersection(Region{ Point{ 0, 0 }, display_info.size });
    if (!region)
        return false;

    if (renderBuffer == nullptr)
    {
        std::cout << "!!!renderBuffer is null!!!" << std::endl;
        return false;
    }

    const auto displaySize = GetDisplaySize();
    const auto renderBufferSize = displaySize.width * displaySize.height * sizeof(uint32_t);

    static std::mutex mutex;
    std::lock_guard lock{ mutex };

    const auto RenderOneWIdOnTop = [=](u64 wId)
    {
        const auto window = Windows::WindowManager->GetWindow(wId);
        if (!window)
        {
            std::cerr << "Window with ID " << wId << " not found." << std::endl;
            return;
        }

        const auto windowRegion = window->GetWindowRegion().GetIntersection(*region);
        if (!windowRegion)
            return;

        Utils::SubView<uint32_t> subView(renderBuffer, renderBufferSize, displaySize, *windowRegion);

        if (!window->GetRegionContent(windowRegion->ToLocal(window->GetPosition()), subView))
        {
            std::cerr << "Failed to get content for window ID " << wId << " in region " << *windowRegion << " at position " << window->GetPosition() << std::endl;
            return;
        }
    };

    // from the bottom to the top
    for (const auto wId : std::ranges::reverse_view(Windows::ZOrder))
    {
        RenderOneWIdOnTop(wId);
    }

    // Render the topmost windows
    for (const auto wId : Windows::TopMostWindows)
    {
        RenderOneWIdOnTop(wId);
    }

    GpuPostBufferRequest post_buffer_request;
    post_buffer_request.display_name = (char *) "default_display";
    post_buffer_request.region.x = region->origin.x;
    post_buffer_request.region.y = region->origin.y;
    post_buffer_request.region.w = region->size.width;
    post_buffer_request.region.h = region->size.height;

    GpuPostBufferResponse post_buffer_response;
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
