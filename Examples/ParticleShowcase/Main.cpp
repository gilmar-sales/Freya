#include <Freya/Freya.hpp>

#include <FreyaExamples/DebugOverlay.hpp>
#include <FreyaExamples/ExampleLogging.hpp>
#include <FreyaExamples/FlyCam.hpp>
#include <FreyaExamples/GroundMesh.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <numbers>
#include <random>
#include <vector>

// ── Particle Showcase ─────────────────────────────────────────────────────────
//
// Demonstrates every new ParticleEmitter / RibbonEmitter feature:
//
//   [1] Campfire      — gravity, drag, turbulence, alpha smoke + rotation
//   [2] Magic Portal  — sphere emitter, angularVelocity, additive
//   [3] Fountain      — cone emitter, gravity, depth-sorted alpha drops
//   [4] Sparkle Ring  — box emitter, flipbook atlas (4×4), angular velocity
//   [5] Burst on key  — SPACE fires a one-shot Burst() of 80 sparks
//   [6] Ribbon trail  — RibbonEmitter following a sine-wave path
//   [7] Sparks        — metallic grinder sparks (high gravity, spread)
//   [8] Bullet traces — fast projectiles leaving RibbonEmitter trails
//   [9] Rain          — velocity-stretched drops, covers the whole scene
//
// Controls: RMB look, WASD move, Space burst / move, Q/E up/down

class MainApp final : public fra::AbstractApplication
{
  public:
    explicit MainApp(const fra::Ref<fra::ServiceProvider>& sp) :
        AbstractApplication(sp)
    {
        auto ws       = GetMainServiceProvider();
        mMeshPool     = sp->GetService<fra::MeshPool>();
        mTexturePool  = sp->GetService<fra::TexturePool>();
        mMaterialPool = sp->GetService<fra::MaterialPool>();
        mLightService = ws->GetService<fra::LightService>();
        mFreyaOptions = ws->GetService<fra::FreyaOptions>();
    }

    void StartUp() override
    {
        mCam.window     = mWindow;
        mCam.cameraPos  = { 0.f, 2.5f, 9.f };
        mCam.yaw        = -90.f;
        mCam.pitch      = -12.f;
        mCam.moveSpeed  = 5.f;
        mCam.blockMouse = [this] { return mOverlay.WantsCaptureMouse(); };
        mCam.BindInput(*mEventManager);

        if (mPlatform)
            mOverlay.Init(*mRenderer, *mWindow, *mPlatform);

        mEventManager->Subscribe<fra::KeyReleasedEvent>(
            [this](const fra::KeyReleasedEvent& ev) {
                if (ev.key == fra::KeyCode::Space)
                {
                    mBurstEmitter.Burst(80u);
                    std::cout << "Burst!\n";
                }
            });

        // Ground plane
        mGroundMesh = FreyaExamples::CreateGroundPlane(*mMeshPool, 18.f,
                                                        glm::vec3(0.15f));
        mGroundMat  = mMaterialPool->Create({
            .albedoFactor    = { 0.18f, 0.18f, 0.18f, 1.f },
            .roughnessFactor = 0.95f,
        });
        {
            fra::Scene::Instance g {};
            g.transform = fra::SceneTransform::FromMatrix(glm::mat4(1.f));
            g.mesh      = mGroundMesh;
            g.material  = mGroundMat;
            g.entityId  = 1;
            g.flags     = fra::kSceneInstanceFlagCastShadows;
            g.mobility  = fra::Mobility::Static;
            mScene.Add(g);
        }

        mLightService->AddLight(fra::MakeDirectionalLight(
            glm::vec3(-0.3f, -1.f, -0.4f), glm::vec3(1.f, 0.95f, 0.85f),
            0.8f));

        // ── [1] Campfire ──────────────────────────────────────────────────
        const glm::vec3 firePos { -3.5f, 0.05f, 0.f };

        mFireFlames.origin             = firePos;
        mFireFlames.velocity           = { 0.f, 1.8f, 0.f };
        mFireFlames.velocityJitter     = { 0.25f, 0.6f, 0.25f };
        mFireFlames.spawnRate          = 60.f;
        mFireFlames.lifetime           = 0.55f;
        mFireFlames.size0              = 0.32f;
        mFireFlames.size1              = 0.06f;
        mFireFlames.color0             = { 1.f, 0.65f, 0.1f, 1.f };
        mFireFlames.color1             = { 0.8f, 0.08f, 0.f, 0.f };
        mFireFlames.blend              = fra::BillboardBlend::Additive;
        mFireFlames.maxParticles       = 128;
        mFireFlames.gravity            = { 0.f, 0.3f, 0.f };
        mFireFlames.drag               = 0.4f;
        mFireFlames.turbulenceStrength = 0.9f;
        mFireFlames.turbulenceScale    = 2.5f;

        mFireSmoke.origin                = firePos + glm::vec3(0.f, 0.3f, 0.f);
        mFireSmoke.velocity              = { 0.f, 0.5f, 0.f };
        mFireSmoke.velocityJitter        = { 0.15f, 0.1f, 0.15f };
        mFireSmoke.spawnRate             = 8.f;
        mFireSmoke.lifetime              = 2.2f;
        mFireSmoke.size0                 = 0.2f;
        mFireSmoke.size1                 = 0.7f;
        mFireSmoke.color0                = { 0.1f, 0.1f, 0.09f, 0.3f };
        mFireSmoke.color1                = { 0.05f, 0.05f, 0.05f, 0.f };
        mFireSmoke.blend                 = fra::BillboardBlend::Alpha;
        mFireSmoke.maxParticles          = 48;
        mFireSmoke.gravity               = { 0.f, 0.1f, 0.f };
        mFireSmoke.drag                  = 0.5f;
        mFireSmoke.angularVelocity       = 0.4f;
        mFireSmoke.angularVelocityJitter = 0.6f;
        mFireSmoke.turbulenceStrength    = 0.35f;
        mFireSmoke.turbulenceScale       = 1.2f;

        mLightService->AddLight(fra::MakePointLight(
            firePos + glm::vec3(0.f, 0.5f, 0.f),
            glm::vec3(1.f, 0.4f, 0.1f), 5.f, 6.f));

        // ── [2] Magic Portal ──────────────────────────────────────────────
        mPortal.origin                = { 0.f, 1.2f, 0.f };
        mPortal.velocity              = { 0.f, 0.f, 0.f };
        mPortal.velocityJitter        = { 0.f, 0.f, 0.f };
        mPortal.spawnRate             = 80.f;
        mPortal.lifetime              = 1.f;
        mPortal.size0                 = 0.12f;
        mPortal.size1                 = 0.04f;
        mPortal.color0                = { 0.5f, 0.9f, 1.f, 1.f };
        mPortal.color1                = { 0.1f, 0.3f, 1.f, 0.f };
        mPortal.blend                 = fra::BillboardBlend::Additive;
        mPortal.maxParticles          = 256;
        mPortal.shape                 = fra::EmitterShape::Sphere;
        mPortal.shapeRadius           = 0.9f;
        mPortal.angularVelocity       = 2.5f;
        mPortal.angularVelocityJitter = 1.5f;
        mPortal.turbulenceStrength    = 0.5f;
        mPortal.turbulenceScale       = 3.f;

        mLightService->AddLight(fra::MakePointLight(
            glm::vec3(0.f, 1.2f, 0.f),
            glm::vec3(0.3f, 0.6f, 1.f), 4.f, 5.f));

        // ── [3] Fountain ──────────────────────────────────────────────────
        mFountain.origin         = { 3.5f, 0.05f, 0.f };
        mFountain.velocity       = { 0.f, 4.5f, 0.f };
        mFountain.velocityJitter = { 0.f, 0.f, 0.f };
        mFountain.spawnRate      = 40.f;
        mFountain.lifetime       = 1.2f;
        mFountain.size0          = 0.06f;
        mFountain.size1          = 0.04f;
        mFountain.color0         = { 0.5f, 0.8f, 1.f, 0.9f };
        mFountain.color1         = { 0.2f, 0.5f, 0.8f, 0.f };
        mFountain.blend          = fra::BillboardBlend::Alpha;
        mFountain.maxParticles   = 128;
        mFountain.shape          = fra::EmitterShape::Cone;
        mFountain.shapeRadius    = 0.18f;
        mFountain.gravity        = { 0.f, -9.8f, 0.f };
        mFountain.drag           = 0.05f;

        // ── [4] Sparkle Ring (flipbook) ───────────────────────────────────
        mSparkle.origin                = { 0.f, 0.1f, -3.5f };
        mSparkle.velocity              = { 0.f, 0.6f, 0.f };
        mSparkle.velocityJitter        = { 0.f, 0.f, 0.f };
        mSparkle.spawnRate             = 50.f;
        mSparkle.lifetime              = 1.2f;
        mSparkle.size0                 = 0.15f;
        mSparkle.size1                 = 0.05f;
        mSparkle.color0                = { 1.f, 0.95f, 0.4f, 1.f };
        mSparkle.color1                = { 1.f, 0.7f, 0.1f, 0.f };
        mSparkle.blend                 = fra::BillboardBlend::Additive;
        mSparkle.maxParticles          = 128;
        mSparkle.shape                 = fra::EmitterShape::Box;
        mSparkle.shapeExtents          = { 0.9f, 0.01f, 0.9f };
        mSparkle.angularVelocity       = 1.5f;
        mSparkle.angularVelocityJitter = 3.f;
        mSparkle.atlasColumns          = 4;
        mSparkle.atlasRows             = 4;
        mSparkle.animFps               = 16.f;
        mSparkle.gravity               = { 0.f, 0.2f, 0.f };

        // ── [5] Burst emitter ─────────────────────────────────────────────
        mBurstEmitter.origin         = { 0.f, 0.1f, 3.5f };
        mBurstEmitter.velocity       = { 0.f, 3.5f, 0.f };
        mBurstEmitter.velocityJitter = { 2.5f, 1.5f, 2.5f };
        mBurstEmitter.spawnRate      = 0.f;
        mBurstEmitter.lifetime       = 1.f;
        mBurstEmitter.size0          = 0.1f;
        mBurstEmitter.size1          = 0.02f;
        mBurstEmitter.color0         = { 1.f, 0.8f, 0.2f, 1.f };
        mBurstEmitter.color1         = { 1.f, 0.3f, 0.f, 0.f };
        mBurstEmitter.blend          = fra::BillboardBlend::Additive;
        mBurstEmitter.maxParticles   = 256;
        mBurstEmitter.gravity        = { 0.f, -4.f, 0.f };
        mBurstEmitter.drag           = 0.3f;
        mBurstEmitter.shape          = fra::EmitterShape::Sphere;
        mBurstEmitter.shapeRadius    = 0.3f;

        // ── [6] Ribbon trail ──────────────────────────────────────────────
        mRibbon.width         = 0.12f;
        mRibbon.pointLifetime = 0.8f;
        mRibbon.minDistance   = 0.04f;
        mRibbon.color0        = { 0.8f, 0.4f, 1.f, 0.9f };
        mRibbon.color1        = { 0.3f, 0.1f, 0.6f, 0.f };
        mRibbon.blend         = fra::BillboardBlend::Additive;
        mRibbon.maxPoints     = 80;

        // ── [7] Sparks (metallic grinder) ─────────────────────────────────
        mSparks.origin             = { -5.f, 0.2f, -4.f };
        mSparks.velocity           = { 1.5f, 4.5f, 0.f };
        mSparks.velocityJitter     = { 3.5f, 2.5f, 3.5f };
        mSparks.spawnRate          = 90.f;
        mSparks.lifetime           = 0.45f;
        mSparks.size0              = 0.045f;
        mSparks.size1              = 0.008f;
        mSparks.color0             = { 1.f, 0.9f, 0.5f, 1.f };
        mSparks.color1             = { 1.f, 0.25f, 0.f, 0.f };
        mSparks.blend              = fra::BillboardBlend::Additive;
        mSparks.maxParticles       = 256;
        mSparks.gravity            = { 0.f, -14.f, 0.f };
        mSparks.drag               = 0.12f;

        mLightService->AddLight(fra::MakePointLight(
            mSparks.origin + glm::vec3(0.f, 0.4f, 0.f),
            glm::vec3(1.f, 0.6f, 0.2f), 2.f, 4.f));

        // ── [8] Bullet traces ─────────────────────────────────────────────
        for (auto& b : mBullets)
        {
            b.ribbon.width         = 0.05f;
            b.ribbon.pointLifetime = 0.18f;
            b.ribbon.minDistance   = 0.09f;
            b.ribbon.color0        = { 1.f, 0.95f, 0.7f, 1.f };
            b.ribbon.color1        = { 1.f, 0.35f, 0.05f, 0.f };
            b.ribbon.blend         = fra::BillboardBlend::Additive;
            b.ribbon.maxPoints     = 28;
        }

        // ── [9] Rain ──────────────────────────────────────────────────────
        mRain.resize(kRainCount);
        std::uniform_real_distribution<float> dXZ { -7.f, 7.f };
        std::uniform_real_distribution<float> dH  { 0.f, kRainTop };
        for (auto& d : mRain)
        {
            d.pos = { dXZ(mRainRng), dH(mRainRng), dXZ(mRainRng) };
            d.t   = 1.f - d.pos.y / kRainTop;
        }

        mFreyaOptions->title = "ParticleShowcase";
        std::cout
            << "ParticleShowcase\n"
               "  [1] Campfire (gravity, drag, turbulence, smoke + rotation)\n"
               "  [2] Magic Portal (sphere, angular velocity, turbulence)\n"
               "  [3] Fountain (cone, gravity, depth sort)\n"
               "  [4] Sparkle Ring (box emitter, flipbook 4x4, angular)\n"
               "  [5] Burst — press SPACE to fire\n"
               "  [6] Ribbon trail — purple sine-wave path\n"
               "  [7] Sparks — metallic grinder sparks (high gravity)\n"
               "  [8] Bullet traces — fast ribbon-trailed projectiles\n"
               "  [9] Rain — velocity-stretched drops, full scene\n"
               "  RMB + WASD to navigate\n";
    }

    void Update() override
    {
        mOverlay.MarkUpdateStart();
        mOverlay.BeginFrame();

        const float dt = mWindow->GetDeltaTime();
        mTime += dt;

        mCam.Update(dt);
        mRenderer->BeginFrame();
        mCam.Apply(*mRenderer);
        mScene.Upload(*mRenderer);

        auto& bb = mRenderer->GetBillboardDraw();

        const glm::vec3 forward  = mCam.Forward();
        const glm::vec3 worldUp  = glm::vec3(0.f, 1.f, 0.f);
        const glm::vec3 camRight =
            glm::normalize(glm::cross(forward, worldUp));
        const glm::vec3 camUp  = glm::cross(camRight, forward);
        const glm::vec3 camPos = mCam.cameraPos;

        // [1] Campfire
        mFireFlames.Tick(dt, bb, camPos);
        mFireSmoke.Tick(dt, bb, camPos);

        // [2] Magic Portal
        mPortal.Tick(dt, bb, camPos);

        // [3] Fountain
        mFountain.Tick(dt, bb, camPos);

        // [4] Sparkle Ring
        mSparkle.Tick(dt, bb, camPos);

        // [5] Burst (Space key)
        mBurstEmitter.Tick(dt, bb, camPos);

        // [6] Ribbon — follows a sine-wave path in XZ plane
        mRibbon.origin = glm::vec3(
            3.5f * std::sin(mTime * 0.8f),
            1.5f + 0.6f * std::sin(mTime * 2.1f),
            -3.5f + 3.5f * std::cos(mTime * 0.6f));
        mRibbon.Tick(dt, bb, camRight, camUp);

        // [7] Sparks
        mSparks.Tick(dt, bb, camPos);

        // [8] Bullet traces — spawn a new projectile periodically
        mBulletTimer += dt;
        if (mBulletTimer >= kBulletInterval)
        {
            mBulletTimer = 0.f;
            for (auto& b : mBullets)
            {
                if (!b.active)
                {
                    b.active    = true;
                    b.timeAlive = 0.f;
                    // Alternate direction and height each shot
                    const float side = (mBulletShot % 2 == 0) ? -1.f : 1.f;
                    const float yOff =
                        0.8f + 0.5f * static_cast<float>(mBulletShot % 3);
                    b.pos = {
                        side * kBulletRange, yOff, -5.5f + mBulletShot % 3
                    };
                    b.vel = { -side * kBulletSpeed, 0.f, 0.f };
                    b.ribbon.Reset();
                    ++mBulletShot;
                    break;
                }
            }
        }
        for (auto& b : mBullets)
        {
            if (!b.active)
                continue;
            b.timeAlive += dt;
            b.pos += b.vel * dt;
            b.ribbon.origin = b.pos;
            b.ribbon.Tick(dt, bb, camRight, camUp);
            if (std::abs(b.pos.x) > kBulletRange + 1.f)
                b.active = false;
        }

        // [9] Rain — advance drops and submit as velocity-stretched quads
        {
            std::uniform_real_distribution<float> dXZ { -7.f, 7.f };
            constexpr float kSpeed = 9.f;
            const glm::vec3 rainVel { 0.f, -kSpeed, 0.f };
            for (auto& d : mRain)
            {
                d.pos.y -= kSpeed * dt;
                d.t = 1.f - d.pos.y / kRainTop;
                if (d.pos.y < -0.2f)
                {
                    d.pos.y = kRainTop;
                    d.pos.x = dXZ(mRainRng);
                    d.pos.z = dXZ(mRainRng);
                    d.t     = 0.f;
                }
                fra::Billboard b;
                b.worldPos             = d.pos;
                b.size                 = { 0.004f, 0.08f };
                b.color = { 0.65f, 0.75f, 1.f, 0.45f * (1.f - d.t) };
                b.align                = fra::BillboardAlign::Screen;
                b.velocityStretch      = true;
                b.velocity             = rainVel;
                b.velocityStretchScale = 0.18f;
                b.blend                = fra::BillboardBlend::Alpha;
                b.depthTest            = false;
                bb.Quad(b);
            }
        }

        const float cpuMs = dt * 1000.f;
        mOverlay.Draw(*mRenderer, *mFreyaOptions, cpuMs,
                      mOverlay.ElapsedUpdateMs(), mLightService.get());
        mOverlay.EndFrame(*mRenderer);
    }

  private:
    fra::Ref<fra::MeshPool>     mMeshPool;
    fra::Ref<fra::TexturePool>  mTexturePool;
    fra::Ref<fra::MaterialPool> mMaterialPool;
    fra::Ref<fra::LightService> mLightService;
    fra::Ref<fra::FreyaOptions> mFreyaOptions;

    fra::MeshHandle     mGroundMesh {};
    fra::MaterialHandle mGroundMat {};
    fra::Scene          mScene;

    // [1] Campfire
    fra::ParticleEmitter mFireFlames;
    fra::ParticleEmitter mFireSmoke;
    // [2] Portal
    fra::ParticleEmitter mPortal;
    // [3] Fountain
    fra::ParticleEmitter mFountain;
    // [4] Sparkle
    fra::ParticleEmitter mSparkle;
    // [5] Burst
    fra::ParticleEmitter mBurstEmitter;
    // [6] Ribbon
    fra::RibbonEmitter mRibbon;
    // [7] Sparks
    fra::ParticleEmitter mSparks;
    // [8] Bullet traces
    struct Bullet
    {
        glm::vec3          pos {};
        glm::vec3          vel {};
        float              timeAlive = 0.f;
        bool               active    = false;
        fra::RibbonEmitter ribbon;
    };
    static constexpr int   kMaxBullets    = 3;
    static constexpr float kBulletInterval = 1.4f;
    static constexpr float kBulletSpeed    = 16.f;
    static constexpr float kBulletRange    = 8.f;
    std::array<Bullet, kMaxBullets> mBullets;
    float        mBulletTimer = kBulletInterval * 0.6f;
    std::uint32_t mBulletShot = 0;
    // [9] Rain
    struct RainDrop
    {
        glm::vec3 pos;
        float     t; // 0=top, 1=bottom
    };
    static constexpr int   kRainCount = 320;
    static constexpr float kRainTop   = 8.f;
    std::vector<RainDrop> mRain;
    std::mt19937          mRainRng { 0xBEEFu };

    float mTime = 0.f;

    FreyaExamples::FlyCam       mCam;
    FreyaExamples::DebugOverlay mOverlay;
};

int main(int, const char**)
{
    return fra::RunApp<MainApp>(
        [](fra::FreyaOptionsBuilder& o) {
            o.SetTitle("ParticleShowcase")
                .SetWidth(1600)
                .SetHeight(900)
                .SetVSync(true)
                .WithReverseZ()
                .SetIblIntensity(0.f)
                .SetEnvironmentMapPath("")
                .SetBloomQuality(fra::BloomQuality::Ultra);
        },
        [](skr::LoggingExtension& l) {
            FreyaExamples::ConfigureLogging(l, "ParticleShowcase.log");
        });
}
