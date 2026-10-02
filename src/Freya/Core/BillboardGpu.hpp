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
        BillboardFlags flags       = BillboardFlags::None;
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
        g.flags = FromBits<BillboardFlags>(
            static_cast<std::uint32_t>(b.align) &
            ToBits(BillboardFlags::AlignMask));

        if (HasFlag(b.flags, BillboardFlags::Sdf))
        {
            g.flags |= BillboardFlags::Sdf;
            g.outlineWidth = b.outlineWidth;
        }
        else if (HasFlag(b.flags, BillboardFlags::Soft))
        {
            g.flags |= BillboardFlags::Soft;
            g.outlineWidth = b.softFadeRange;
        }
        else
        {
            g.outlineWidth = b.outlineWidth;
        }

        if (HasFlag(b.flags, BillboardFlags::ScreenSize))
            g.flags |= BillboardFlags::ScreenSize;

        // aux: velocity-stretch takes priority; otherwise axis for
        // FixedAxis/Planar.
        if (HasFlag(b.flags, BillboardFlags::VelocityStretch))
        {
            g.flags |= BillboardFlags::VelocityStretch;
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

    /**
     * @brief GPU instance for one ConnectedBillboard (std430).
     *
     * Corners are vec4 (w unused) to keep std430 alignment trivial.
     * Keep in sync with billboard_connected.vert.
     */
    struct ConnectedBillboardGpuInstance
    {
        glm::vec4      c0 { 0.f };
        glm::vec4      c1 { 0.f };
        glm::vec4      c2 { 0.f };
        glm::vec4      c3 { 0.f };
        glm::vec4      color0 { 1.f };
        glm::vec4      color1 { 1.f };
        glm::vec4      uvRect { 0.f, 0.f, 1.f, 1.f };
        std::uint32_t  textureIndex = 0;
        BillboardFlags flags        = BillboardFlags::None;
        float         clipMax      = 1.f;
        float         pad          = 0.f;
    };

    static_assert(sizeof(ConnectedBillboardGpuInstance) == 128,
                  "ConnectedBillboardGpuInstance must match GLSL "
                  "std430");

    [[nodiscard]] inline ConnectedBillboardGpuInstance
    ToConnectedBillboardGpu(const ConnectedBillboard& b)
    {
        ConnectedBillboardGpuInstance g {};
        g.c0           = glm::vec4(b.c0, 0.f);
        g.c1           = glm::vec4(b.c1, 0.f);
        g.c2           = glm::vec4(b.c2, 0.f);
        g.c3           = glm::vec4(b.c3, 0.f);
        g.color0       = b.color0;
        g.color1       = b.color1;
        g.uvRect       = b.uvRect;
        g.textureIndex = b.textureIndex;
        g.clipMax      = b.clipMax;
        return g;
    }

} // namespace FREYA_NAMESPACE
