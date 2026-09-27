#pragma once

#include "Freya/Core/BillboardDraw.hpp"

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Spline-based rope/vine renderer using connected billboards.
     *
     * Evaluates a Catmull-Rom spline through @c controlPoints and submits
     * the tessellated centerline as one seamless camera-facing ribbon via
     * BillboardDraw::Strip. Consecutive segments share mitered edge
     * vertices exactly, so joints have no gaps or double-blended
     * overlaps.
     *
     * Set @c growT in [0, 1] to animate growth from root to tip.
     */
    class SplineRope
    {
      public:
        std::vector<glm::vec3> controlPoints;

        float baseRadius = 0.06f; ///< Radius at the first control point.
        float tipRadius  = 0.02f; ///< Radius at the last control point.

        /// [0, 1] — visible portion of the rope (0 = hidden, 1 = full).
        float growT = 1.0f;

        glm::vec4      color0 { 0.28f, 0.72f, 0.22f, 1.0f };
        glm::vec4      color1 { 0.12f, 0.45f, 0.10f, 1.0f };
        BillboardBlend blend        = BillboardBlend::Alpha;
        BillboardLayer layer        = BillboardLayer::Vfx;
        bool           depthTest    = true;
        std::uint32_t  textureIndex = 0;
        std::uint32_t  segments     = 32; ///< Tessellation step count.
        /// Texture repeats along the rope (u spans [0, uvRepeat]).
        float uvRepeat = 1.0f;
        /// Miter-limit multiple of the local half-width (see StripStyle).
        float miterLimit = 2.5f;

        /**
         * @brief Evaluate spline and push a connected strip into @p draw.
         *
         * @p camRight / @p camUp are the camera's world-space right and up
         * vectors (from the view matrix or FlyCam yaw/pitch). They span
         * the screen plane the ribbon is built in.
         *
         * No-op when @c controlPoints.size() < 2 or @c growT <= 0.
         */
        void Submit(BillboardDraw&   draw,
                    const glm::vec3& camRight,
                    const glm::vec3& camUp) const;
    };

} // namespace FREYA_NAMESPACE
