#pragma once

/**
 * @file BillboardGpu.hpp
 * @brief Internal std430 billboard instance layout (BillboardPass).
 */

#include "Freya/Core/BillboardDraw.hpp"

#include <cstdint>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief GPU instance (std430). Keep in sync with billboard.vert.
     */
    struct BillboardGpuInstance
    {
        glm::vec3     worldPos { 0.f };
        float         clipMax = 1.f;
        glm::vec2     size { 1.f };
        std::uint32_t textureIndex = 0;
        std::uint32_t flags        = 0;
        glm::vec4     color { 1.f };
        glm::vec4     uvRect { 0.f, 0.f, 1.f, 1.f };
        glm::vec2     localOffset { 0.f };
        float         outlineWidth = 0.f;
        float         _pad         = 0.f;
        glm::vec4     outlineColor { 0.f, 0.f, 0.f, 1.f };
    };

    static_assert(sizeof(BillboardGpuInstance) == 96,
                  "BillboardGpuInstance must match GLSL std430");

    [[nodiscard]] inline BillboardGpuInstance ToBillboardGpu(const Billboard& b)
    {
        BillboardGpuInstance g {};
        g.worldPos     = b.worldPos;
        g.clipMax      = b.clipMax;
        g.size         = b.size;
        g.textureIndex = b.textureIndex;
        g.flags        = 0;
        if (b.align == BillboardAlign::Cylindrical)
            g.flags |= kBillboardFlagCylindrical;
        if (b.sdf)
            g.flags |= kBillboardFlagSdf;
        g.color        = b.color;
        g.uvRect       = b.uvRect;
        g.localOffset  = b.localOffset;
        g.outlineWidth = b.outlineWidth;
        g.outlineColor = b.outlineColor;
        return g;
    }

} // namespace FREYA_NAMESPACE
