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
        float     rotation = 0.f; ///< Screen-space rotation radians (ex _pad)
        glm::vec4 outlineColor { 0.f, 0.f, 0.f, 1.f };
        /// FixedAxis/Planar: (axisUp, 0).  VelocityStretch: (vel, scale).
        glm::vec4 aux { 0.f };
    };

    static_assert(sizeof(BillboardGpuInstance) == 112,
                  "BillboardGpuInstance must match GLSL std430");

    [[nodiscard]] inline BillboardGpuInstance ToBillboardGpu(const Billboard& b)
    {
        BillboardGpuInstance g {};
        g.worldPos     = b.worldPos;
        g.clipMax      = b.clipMax;
        g.size         = b.size;
        g.textureIndex = b.textureIndex;

        // Bits 0-2: alignment type.
        g.flags =
            static_cast<std::uint32_t>(b.align) & kBillboardAlignMask;

        if (b.sdf)
        {
            g.flags |= kBillboardFlagSdf;
            g.outlineWidth = b.outlineWidth;
        }
        else if (b.softParticle)
        {
            g.flags |= kBillboardFlagSoft;
            g.outlineWidth = b.softFadeRange;
        }
        else
        {
            g.outlineWidth = b.outlineWidth;
        }

        if (b.screenSpaceSize)
            g.flags |= kBillboardFlagScreenSize;

        // aux: velocity-stretch takes priority; otherwise axis for
        // FixedAxis/Planar.
        if (b.velocityStretch)
        {
            g.flags |= kBillboardFlagVelocityStretch;
            g.aux = glm::vec4(b.velocity, b.velocityStretchScale);
        }
        else if (b.align == BillboardAlign::FixedAxis ||
                 b.align == BillboardAlign::Planar)
        {
            g.aux = glm::vec4(b.axisUp, 0.f);
        }

        g.color        = b.color;
        g.uvRect       = b.uvRect;
        g.localOffset  = b.localOffset;
        g.outlineColor = b.outlineColor;
        g.rotation     = b.rotation;
        return g;
    }

} // namespace FREYA_NAMESPACE
