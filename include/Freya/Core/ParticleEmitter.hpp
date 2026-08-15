#pragma once

#include "Freya/Core/BillboardDraw.hpp"

#include <cstdint>
#include <random>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    struct ParticleDesc
    {
        glm::vec3 pos { 0.f };
        glm::vec3 vel { 0.f };
        float     age      = 0.f;
        float     lifetime = 1.f;
        float     size0    = 0.1f;
        float     size1    = 0.02f;
        glm::vec4 color0 { 1.f };
        glm::vec4 color1 { 1.f, 1.f, 1.f, 0.f };
    };

    /**
     * @brief CPU particle emitter that pushes additive/alpha billboards.
     */
    class ParticleEmitter
    {
      public:
        glm::vec3      origin { 0.f };
        glm::vec3      velocity { 0.f, 1.2f, 0.f };
        glm::vec3      velocityJitter { 0.35f, 0.25f, 0.35f };
        float          spawnRate = 24.f;
        float          lifetime  = 0.7f;
        float          size0     = 0.12f;
        float          size1     = 0.02f;
        glm::vec4      color0 { 0.35f, 0.85f, 1.f, 1.f };
        glm::vec4      color1 { 0.1f, 0.2f, 1.f, 0.f };
        BillboardBlend blend         = BillboardBlend::Additive;
        std::uint32_t  textureIndex  = 0;
        std::uint32_t  maxParticles  = 256;

        void Tick(float dt, BillboardDraw& draw);

      private:
        float                    mAccum = 0.f;
        std::vector<ParticleDesc> mLive;
        std::mt19937             mRng { 0xC0FFEEu };
    };

} // namespace FREYA_NAMESPACE
