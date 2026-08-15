#include "Freya/Core/ParticleEmitter.hpp"

#include <algorithm>

namespace FREYA_NAMESPACE
{
    void ParticleEmitter::Tick(const float dt, BillboardDraw& draw)
    {
        if (dt <= 0.f)
            return;

        mAccum += spawnRate * dt;
        const auto toSpawn = static_cast<int>(mAccum);
        mAccum -= static_cast<float>(toSpawn);

        std::uniform_real_distribution<float> jitter(-1.f, 1.f);

        for (int i = 0; i < toSpawn; ++i)
        {
            if (mLive.size() >= maxParticles)
                break;
            ParticleDesc p {};
            p.pos = origin;
            p.vel =
                velocity + glm::vec3(jitter(mRng), jitter(mRng), jitter(mRng)) *
                               velocityJitter;
            p.lifetime = lifetime;
            p.size0    = size0;
            p.size1    = size1;
            p.color0   = color0;
            p.color1   = color1;
            mLive.push_back(p);
        }

        for (auto& p : mLive)
            p.age += dt;

        std::erase_if(mLive, [](const ParticleDesc& p) {
            return p.age >= p.lifetime;
        });

        for (const auto& p : mLive)
        {
            const float t = std::clamp(p.age / p.lifetime, 0.f, 1.f);
            Billboard   b {};
            b.worldPos     = p.pos + p.vel * p.age;
            const float sz = glm::mix(p.size0, p.size1, t);
            b.size         = { sz, sz };
            b.color        = glm::mix(p.color0, p.color1, t);
            b.textureIndex = textureIndex;
            b.align        = BillboardAlign::Screen;
            b.blend        = blend;
            b.layer        = BillboardLayer::Vfx;
            b.depthTest    = true;
            draw.Quad(b);
        }
    }

} // namespace FREYA_NAMESPACE
