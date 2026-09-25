#pragma once

#include "Freya/Core/BillboardDraw.hpp"

#include <cstdint>
#include <random>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    enum class EmitterShape : std::uint8_t
    {
        Point  = 0, ///< All particles spawn at origin.
        Sphere = 1, ///< Random point inside a sphere.
        Box    = 2, ///< Random point inside an axis-aligned box.
        Cone   = 3, ///< Random point on a disk perpendicular to velocity.
    };

    struct ParticleDesc
    {
        glm::vec3 pos { 0.f };
        glm::vec3 vel { 0.f };
        float     age        = 0.f;
        float     lifetime   = 1.f;
        float     size0      = 0.1f;
        float     size1      = 0.02f;
        float     rotation   = 0.f;
        float     angularVel = 0.f;
        glm::vec4 color0 { 1.f };
        glm::vec4 color1 { 1.f, 1.f, 1.f, 0.f };
    };

    /**
     * @brief CPU particle emitter that pushes additive/alpha billboards.
     *
     * Not synchronized: call Tick from one thread per emitter. Concurrent
     * Tick on different emitters sharing one BillboardDraw is fine
     * (BillboardDraw is thread-safe).
     */
    class ParticleEmitter
    {
      public:
        // ── Spawn ──────────────────────────────────────────────────────────
        glm::vec3      origin { 0.f };
        glm::vec3      velocity { 0.f, 1.2f, 0.f };
        glm::vec3      velocityJitter { 0.35f, 0.25f, 0.35f };
        float          spawnRate = 24.f;
        float          lifetime  = 0.7f;
        float          size0     = 0.12f;
        float          size1     = 0.02f;
        glm::vec4      color0 { 0.35f, 0.85f, 1.f, 1.f };
        glm::vec4      color1 { 0.1f, 0.2f, 1.f, 0.f };
        BillboardBlend blend        = BillboardBlend::Additive;
        std::uint32_t  textureIndex = 0;
        std::uint32_t  maxParticles = 256;

        // ── Forces ─────────────────────────────────────────────────────────
        glm::vec3 gravity { 0.f }; ///< World-space acceleration (m/s²).
        float     drag = 0.f;      ///< Velocity damping per second [0, 1].

        // ── Rotation ───────────────────────────────────────────────────────
        float angularVelocity       = 0.f; ///< Base angular speed (rad/s).
        float angularVelocityJitter = 0.f; ///< ±jitter added at spawn.

        // ── Turbulence ─────────────────────────────────────────────────────
        float turbulenceStrength = 0.f; ///< Curl noise velocity scale.
        float turbulenceScale    = 1.f; ///< Spatial frequency of the field.
        float turbulenceSpeed    = 1.f; ///< Time speed of the field.

        // ── Emitter shape ──────────────────────────────────────────────────
        EmitterShape shape          = EmitterShape::Point;
        float        shapeRadius    = 0.5f; ///< Sphere/Cone disk radius.
        float        shapeConeAngle = 30.f; ///< Cone half-angle (deg).
        glm::vec3    shapeExtents { 0.5f }; ///< Box half-extents.

        // ── Flipbook atlas ─────────────────────────────────────────────────
        std::uint32_t atlasColumns = 1;    ///< Horizontal tile count.
        std::uint32_t atlasRows    = 1;    ///< Vertical tile count.
        float         animFps      = 12.f; ///< Atlas playback rate.

        /**
         * @brief Advance simulation and submit billboards.
         *
         * @param cameraPos Used for back-to-front depth sort on Alpha blend.
         */
        void Tick(float dt, BillboardDraw& draw,
                  glm::vec3 cameraPos = glm::vec3(0.f));

        /// Spawn @p count particles in the next Tick (one-shot burst).
        void Burst(std::uint32_t count);

      private:
        glm::vec3 SampleSpawnOffset(std::mt19937& rng) const;

        float                     mAccum        = 0.f;
        float                     mNoiseTime    = 0.f;
        std::uint32_t             mBurstPending = 0u;
        std::vector<ParticleDesc> mLive;
    };

} // namespace FREYA_NAMESPACE
