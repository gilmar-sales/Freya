#pragma once

#include "Freya/Core/BillboardDraw.hpp"

#include <cstdint>
#include <limits>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Trail/ribbon renderer built on top of BillboardDraw.
     *
     * Call Tick every frame with the current world position (origin).
     * The emitter records a ring of trail points; consecutive pairs are
     * submitted as stretched, rotated billboards oriented toward the camera.
     *
     * Thread model: same as ParticleEmitter — one thread per emitter,
     * concurrent emitters sharing a BillboardDraw are fine.
     */
    class RibbonEmitter
    {
      public:
        glm::vec3      origin { 0.f };     ///< Updated by the caller each frame.
        float          width         = 0.2f;
        float          pointLifetime = 0.5f; ///< Seconds before a point fades.
        float          minDistance   = 0.05f; ///< Min movement to record a point.
        glm::vec4      color0 { 1.f };        ///< Color at freshest point.
        glm::vec4      color1 { 1.f, 1.f, 1.f, 0.f }; ///< Color at oldest point.
        BillboardBlend blend        = BillboardBlend::Additive;
        std::uint32_t  textureIndex = 0;
        std::uint32_t  maxPoints    = 64;

        /**
         * @brief Advance trail and submit billboard segments.
         *
         * @param cameraRight  Camera right vector (first row of view matrix).
         * @param cameraUp     Camera up vector (second row of view matrix).
         */
        void Tick(float dt, BillboardDraw& draw, const glm::vec3& cameraRight,
                  const glm::vec3& cameraUp);

        void Reset();

      private:
        struct Point
        {
            glm::vec3 pos;
            float     age;
            float     lifetime;
        };

        std::vector<Point> mPoints;
        glm::vec3          mLastPos {
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()
        };
    };

} // namespace FREYA_NAMESPACE
