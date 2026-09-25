#include "Freya/Core/ParticleEmitter.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include <glm/gtc/constants.hpp>

namespace FREYA_NAMESPACE
{
    thread_local std::mt19937 ParticlesRandomEngine { 0xC0FFEEu };

    // ── Turbulence helpers ────────────────────────────────────────────────

    namespace
    {
        // Three independent trig-based "smooth noise" functions used as a
        // potential field ψ = (n1, n2, n3). Their curl is divergence-free.
        float N1(glm::vec3 p)
        {
            return std::sin(p.x * 1.7f + p.y * 2.3f) *
                   std::cos(p.y * 1.3f + p.z * 1.9f) *
                   std::sin(p.z * 2.1f + p.x * 0.8f);
        }
        float N2(glm::vec3 p)
        {
            return std::sin(p.y * 1.5f + p.z * 0.9f) *
                   std::cos(p.z * 2.0f + p.x * 1.6f) *
                   std::sin(p.x * 1.2f + p.y * 2.7f);
        }
        float N3(glm::vec3 p)
        {
            return std::sin(p.z * 1.3f + p.x * 2.1f) *
                   std::cos(p.x * 0.7f + p.y * 1.4f) *
                   std::sin(p.y * 2.5f + p.z * 1.1f);
        }

        // Returns curl(ψ) = divergence-free velocity perturbation.
        glm::vec3 CurlNoise(glm::vec3 p, float t, float scale)
        {
            p *= scale;
            p.x += t * 0.15f;
            p.z -= t * 0.12f;

            const float e     = 0.1f;
            const float inv2e = 0.5f / e;

            float dn3dy =
                (N3(p + glm::vec3(0, e, 0)) - N3(p - glm::vec3(0, e, 0))) *
                inv2e;
            float dn2dz =
                (N2(p + glm::vec3(0, 0, e)) - N2(p - glm::vec3(0, 0, e))) *
                inv2e;
            float dn1dz =
                (N1(p + glm::vec3(0, 0, e)) - N1(p - glm::vec3(0, 0, e))) *
                inv2e;
            float dn3dx =
                (N3(p + glm::vec3(e, 0, 0)) - N3(p - glm::vec3(e, 0, 0))) *
                inv2e;
            float dn2dx =
                (N2(p + glm::vec3(e, 0, 0)) - N2(p - glm::vec3(e, 0, 0))) *
                inv2e;
            float dn1dy =
                (N1(p + glm::vec3(0, e, 0)) - N1(p - glm::vec3(0, e, 0))) *
                inv2e;

            return glm::vec3(dn3dy - dn2dz, dn1dz - dn3dx, dn2dx - dn1dy);
        }
    } // namespace

    // ── Shape sampling ────────────────────────────────────────────────────

    glm::vec3 ParticleEmitter::SampleSpawnOffset(std::mt19937& rng) const
    {
        std::uniform_real_distribution<float> u01(0.f, 1.f);
        std::uniform_real_distribution<float> neg1to1(-1.f, 1.f);

        switch (shape)
        {
            case EmitterShape::Sphere: {
                glm::vec3 v;
                float     sq;
                do
                {
                    v  = glm::vec3(neg1to1(rng), neg1to1(rng), neg1to1(rng));
                    sq = glm::dot(v, v);
                } while (sq > 1.f || sq < 1e-12f);
                const float r = shapeRadius * std::cbrt(u01(rng));
                return glm::normalize(v) * r;
            }
            case EmitterShape::Box: {
                return glm::vec3(neg1to1(rng), neg1to1(rng), neg1to1(rng)) *
                       shapeExtents;
            }
            case EmitterShape::Cone: {
                // Random point on a disk perpendicular to velocity.
                const glm::vec3 vDir =
                    glm::length(velocity) > 1e-6f ? glm::normalize(velocity)
                                                  : glm::vec3(0.f, 1.f, 0.f);
                const glm::vec3 tmp =
                    (std::abs(vDir.x) < 0.9f) ? glm::vec3(1.f, 0.f, 0.f)
                                              : glm::vec3(0.f, 1.f, 0.f);
                const glm::vec3 right = glm::normalize(glm::cross(vDir, tmp));
                const glm::vec3 up    = glm::cross(vDir, right);

                const float phi = u01(rng) * glm::two_pi<float>();
                const float r   = shapeRadius * std::sqrt(u01(rng));
                return right * (r * std::cos(phi)) + up * (r * std::sin(phi));
            }
            default: // Point
                return glm::vec3(0.f);
        }
    }

    // ── Public API ────────────────────────────────────────────────────────

    void ParticleEmitter::Burst(const std::uint32_t count)
    {
        mBurstPending += count;
    }

    void ParticleEmitter::Tick(const float     dt,
                               BillboardDraw&  draw,
                               const glm::vec3 cameraPos)
    {
        if (dt <= 0.f)
            return;

        mNoiseTime += dt * turbulenceSpeed;

        std::uniform_real_distribution<float> jitter(-1.f, 1.f);

        // ── Spawn from continuous rate ──
        mAccum += spawnRate * dt;
        auto toSpawn = static_cast<std::uint32_t>(mAccum);
        mAccum -= static_cast<float>(toSpawn);

        // ── Consume burst ──
        toSpawn += mBurstPending;
        mBurstPending = 0u;

        for (std::uint32_t i = 0; i < toSpawn; ++i)
        {
            if (mLive.size() >= maxParticles)
                break;
            ParticleDesc p {};
            p.pos        = origin + SampleSpawnOffset(ParticlesRandomEngine);
            p.vel        = velocity + glm::vec3(jitter(ParticlesRandomEngine),
                                                jitter(ParticlesRandomEngine),
                                                jitter(ParticlesRandomEngine)) *
                                          velocityJitter;
            p.lifetime   = lifetime;
            p.size0      = size0;
            p.size1      = size1;
            p.color0     = color0;
            p.color1     = color1;
            p.rotation   = 0.f;
            p.angularVel = angularVelocity + jitter(ParticlesRandomEngine) *
                                                 angularVelocityJitter;
            mLive.push_back(p);
        }

        // ── Physics & age ──
        const float dragFactor = std::max(0.f, 1.f - drag * dt);
        for (auto& p : mLive)
        {
            p.age += dt;
            p.vel += gravity * dt;
            p.vel *= dragFactor;

            if (turbulenceStrength > 0.f)
            {
                p.vel += CurlNoise(p.pos, mNoiseTime, turbulenceScale) *
                         turbulenceStrength * dt;
            }

            p.pos += p.vel * dt;
            p.rotation += p.angularVel * dt;
        }

        std::erase_if(mLive, [](const ParticleDesc& p) {
            return p.age >= p.lifetime;
        });

        // ── Build billboard batch ──
        std::vector<Billboard> batch;
        batch.reserve(mLive.size());

        const bool  useAtlas    = (atlasColumns > 1 || atlasRows > 1);
        const float totalFrames = static_cast<float>(atlasColumns * atlasRows);

        for (const auto& p : mLive)
        {
            const float t  = std::clamp(p.age / p.lifetime, 0.f, 1.f);
            const float sz = glm::mix(p.size0, p.size1, t);

            Billboard b {};
            b.worldPos     = p.pos;
            b.size         = { sz, sz };
            b.color        = glm::mix(p.color0, p.color1, t);
            b.textureIndex = textureIndex;
            b.align        = BillboardAlign::Screen;
            b.blend        = blend;
            b.layer        = BillboardLayer::Vfx;
            b.depthTest    = true;
            b.rotation     = p.rotation;

            if (useAtlas)
            {
                const float frame = std::fmod(p.age * animFps, totalFrames);
                const auto  fi    = static_cast<std::uint32_t>(frame);
                const auto  col   = static_cast<float>(fi % atlasColumns);
                const auto  row   = static_cast<float>(fi / atlasColumns);
                const float fw    = 1.f / static_cast<float>(atlasColumns);
                const float fh    = 1.f / static_cast<float>(atlasRows);
                b.uvRect          = { col * fw, row * fh, (col + 1.f) * fw,
                                      (row + 1.f) * fh };
            }

            batch.push_back(b);
        }

        // ── Depth sort (Alpha only, back-to-front) ──
        if (blend == BillboardBlend::Alpha && !batch.empty())
        {
            std::sort(batch.begin(), batch.end(),
                      [&cameraPos](const Billboard& a, const Billboard& b) {
                          const glm::vec3 da = a.worldPos - cameraPos;
                          const glm::vec3 db = b.worldPos - cameraPos;
                          return glm::dot(da, da) > glm::dot(db, db);
                      });
        }

        if (!batch.empty())
            draw.Quads(batch);
    }

} // namespace FREYA_NAMESPACE
