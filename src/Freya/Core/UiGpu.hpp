#pragma once

/**
 * @file UiGpu.hpp
 * @brief Internal std430 UI instance layout (UiPass).
 */

#include "Freya/Core/UiDraw.hpp"

#include <cstdint>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief GPU instance (std430). Keep in sync with ui.vert.
     */
    struct UiGpuInstance
    {
        glm::vec4     rect { 0.f }; ///< xywh framebuffer / logical pixels
        glm::vec4     uvRect { 0.f, 0.f, 1.f, 1.f };
        glm::vec4     color { 1.f };
        std::uint32_t textureIndex = 0;
        std::uint32_t flags        = 0;
        float         rounding     = 0.f;
        float         borderWidth  = 0.f;
        float         clipMax      = 1.f;
        float         outlineWidth = 0.f;
        float         z            = 0.f;
        float         _pad         = 0.f;
        glm::vec4     outlineColor { 0.f, 0.f, 0.f, 1.f };
    };

    static_assert(sizeof(UiGpuInstance) == 96,
                  "UiGpuInstance must match GLSL std430");

    [[nodiscard]] inline UiGpuInstance ToUiGpu(const UiQuad& q)
    {
        UiGpuInstance g {};
        g.rect         = q.rect;
        g.uvRect       = q.uvRect;
        g.color        = q.color;
        g.textureIndex = q.textureIndex;
        g.flags        = q.flags;
        g.rounding     = q.rounding;
        g.borderWidth  = q.borderWidth;
        g.clipMax      = q.clipMax;
        g.outlineWidth = q.outlineWidth;
        g.z            = q.z;
        g.outlineColor = q.outlineColor;
        return g;
    }

} // namespace FREYA_NAMESPACE
