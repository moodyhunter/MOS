// SPDX-License-Identifier: GPL-3.0-or-later

#include "proto/graphics-gpu.service.h"
#include "utils/common.hpp"

#include <memory>

namespace DisplayManager::Render
{
    struct DisplayInfo
    {
        int width;
        int height;
    };

    class RendererClass
    {
      public:
        explicit RendererClass();
        ~RendererClass();

        bool Initialize();

        bool RenderFullScreen();

        void SetCursorPosition(int x, int y);

        bool DamageGlobal(const Region &region);

      private:
        bool DoPostBuffer(const Region &region, pb_bytes_array_t *content);

      private:
        std::unique_ptr<GraphicsManagerStub> graphics_manager;
        Render::DisplayInfo display_info;
        pb_bytes_array_t *back_buffer = nullptr; ///< background buffer for the entire display
    };

    inline uint32_t AlphaBlend(uint32_t base, uint32_t overlay)
    {
        const auto baseAlpha = (base >> 24) & 0xff;
        const auto overlayAlpha = (overlay >> 24) & 0xff;

        if (overlayAlpha == 0)
            return base; // No overlay, return base

        if (baseAlpha == 0)
            return overlay; // Base is transparent, return overlay

        const auto invOverlayAlpha = 255 - overlayAlpha;
        const auto r = ((base >> 16) & 0xff) * invOverlayAlpha + ((overlay >> 16) & 0xff) * overlayAlpha;
        const auto g = ((base >> 8) & 0xff) * invOverlayAlpha + ((overlay >> 8) & 0xff) * overlayAlpha;
        const auto b = (base & 0xff) * invOverlayAlpha + (overlay & 0xff) * overlayAlpha;

        return (255 << 24) | ((r / 255) << 16) | ((g / 255) << 8) | (b / 255);
    }

    const inline std::unique_ptr<RendererClass> Renderer = std::make_unique<RendererClass>();
} // namespace DisplayManager::Render
