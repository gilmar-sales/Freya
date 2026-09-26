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
#include <string>
#include <vector>

// ── Billboard Showcase ────────────────────────────────────────────────────────
//
// 8 billboard types arranged in a single visible row, plus a player/enemy
// nameplate panel and MU Online-style animated damage numbers.
//
// Demo row (z = 6):
//   x = -3.5  [1] Screen      — standard view-plane quad
//   x = -2.5  [2] Cylindrical — yaw-only (stays upright)
//   x = -1.5  [3] Spherical   — per-instance point-toward-camera
//   x = -0.5  [4] FixedAxis   — axisUp = diagonal (0.6, 0, 0.8)
//   x = +0.5  [5] Planar      — flat decal on ground
//   x = +1.5  [6] ScreenSize  — constant-pixel waypoint icon
//   x = +2.5  [7] VelocityStretch — sparks stretched by velocity
//   x = +3.5  [8] SoftParticle   — depth-fade near the ground
//
// Centre panel (z = 0):
//   x = -2    [9]  Player (DarkKnight) — sprite + HP bar + nameplate
//   x = +2    [10] Enemy  (Balgass)    — sprite + HP bar + nameplate
//   [11] Animated damage numbers (Normal, Crit, Skill, Miss, Heal)
//
// Controls: RMB + WASD to fly, pitch/yaw with mouse.

enum class DmgType : std::uint8_t
{
    Normal,
    Critical,
    Skill,
    Miss,
    Heal,
};

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
        mCam.cameraPos  = { 0.f, 2.5f, 12.f };
        mCam.yaw        = -90.f;
        mCam.pitch      = -8.f;
        mCam.moveSpeed  = 5.f;
        mCam.blockMouse = [this] { return mOverlay.WantsCaptureMouse(); };
        mCam.BindInput(*mEventManager);

        if (mPlatform)
            mOverlay.Init(*mRenderer, *mWindow, *mPlatform);

        mGroundMesh = FreyaExamples::CreateGroundPlane(
            *mMeshPool, 20.f, glm::vec3(0.15f));
        mGroundMat = mMaterialPool->Create({
            .albedoFactor    = { 0.18f, 0.18f, 0.18f, 1.f },
            .roughnessFactor = 0.9f,
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
            glm::vec3(-0.3f, -1.f, -0.4f),
            glm::vec3(1.f, 0.95f, 0.85f), 0.8f));

        // Orbit angles for VelocityStretch sparks
        mOrbitAngles.resize(8);
        for (int i = 0; i < 8; ++i)
            mOrbitAngles[i] =
                i * (2.f * std::numbers::pi_v<float> / 8.f);

        // Font for SDF nameplates / damage numbers
        mFont = fra::FontAtlas::Create(
            *mTexturePool, "./Resources/Fonts/NotoSans-Regular.ttf");
        if (!mFont.Valid())
            std::cerr << "[BillboardShowcase] font not found — "
                         "text disabled\n";

        std::cout
            << "BillboardShowcase ready\n"
               "  Demo row (z=6): Screen | Cyl | Spherical | FixedAxis |"
               " Planar | ScreenSize | VelStretch | Soft\n"
               "  Centre (z=0): player + enemy nameplates + damage numbers\n"
               "  RMB + WASD to fly\n";
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

        // ── Demo row at z = 6, y = 1.8 ───────────────────────────────────
        // Each slot is spaced 1 m apart: x = -3.5 … +3.5.
        // Three stacked quads (y -0.4 / 0 / +0.4) at 0.55 × 0.55 m.

        // [1] Screen
        {
            const glm::vec3 base { -3.5f, 1.8f, 6.f };
            for (int i = -1; i <= 1; ++i)
            {
                fra::Billboard b;
                b.worldPos = base + glm::vec3(0.f, i * 0.4f, 0.f);
                b.size     = { 0.55f, 0.55f };
                b.color    = { 0.3f, 0.55f, 1.f, 0.95f };
                b.align    = fra::BillboardAlign::Screen;
                b.blend    = fra::BillboardBlend::Alpha;
                bb.Quad(b);
            }
            if (mFont.Valid())
                bb.Text(base + glm::vec3(0.f, -1.0f, 0.f),
                        "Screen", mFont, 0.18f,
                        { 0.6f, 0.8f, 1.f, 1.f }, 1.5f,
                        { 0.f, 0.f, 0.f, 1.f },
                        fra::BillboardAlign::Screen,
                        fra::BillboardLayer::Vfx);
        }

        // [2] Cylindrical
        {
            const glm::vec3 base { -2.5f, 1.8f, 6.f };
            for (int i = -1; i <= 1; ++i)
            {
                fra::Billboard b;
                b.worldPos = base + glm::vec3(0.f, i * 0.4f, 0.f);
                b.size     = { 0.55f, 0.55f };
                b.color    = { 0.25f, 1.f, 0.45f, 0.95f };
                b.align    = fra::BillboardAlign::Cylindrical;
                b.blend    = fra::BillboardBlend::Alpha;
                bb.Quad(b);
            }
            if (mFont.Valid())
                bb.Text(base + glm::vec3(0.f, -1.0f, 0.f),
                        "Cyl", mFont, 0.18f,
                        { 0.5f, 1.f, 0.6f, 1.f }, 1.5f,
                        { 0.f, 0.f, 0.f, 1.f },
                        fra::BillboardAlign::Screen,
                        fra::BillboardLayer::Vfx);
        }

        // [3] Spherical
        {
            const glm::vec3 base { -1.5f, 1.8f, 6.f };
            for (int i = -1; i <= 1; ++i)
            {
                fra::Billboard b;
                b.worldPos = base + glm::vec3(0.f, i * 0.4f, 0.f);
                b.size     = { 0.55f, 0.55f };
                b.color    = { 1.f, 0.35f, 0.9f, 0.95f };
                b.align    = fra::BillboardAlign::Spherical;
                b.blend    = fra::BillboardBlend::Alpha;
                bb.Quad(b);
            }
            if (mFont.Valid())
                bb.Text(base + glm::vec3(0.f, -1.0f, 0.f),
                        "Sph", mFont, 0.18f,
                        { 1.f, 0.5f, 0.9f, 1.f }, 1.5f,
                        { 0.f, 0.f, 0.f, 1.f },
                        fra::BillboardAlign::Screen,
                        fra::BillboardLayer::Vfx);
        }

        // [4] FixedAxis — axisUp = (0, 1, 0) tilted with Z offset so
        //     the orientation is visually distinct from Cylindrical.
        {
            const glm::vec3 base { -0.5f, 1.8f, 6.f };
            for (int i = -1; i <= 1; ++i)
            {
                fra::Billboard b;
                b.worldPos = base + glm::vec3(0.f, i * 0.4f, 0.f);
                b.size     = { 0.55f, 0.55f };
                b.color    = { 0.2f, 0.9f, 1.f, 0.95f };
                b.align    = fra::BillboardAlign::FixedAxis;
                b.axisUp   = glm::normalize(glm::vec3(0.f, 1.f, 0.f));
                b.blend    = fra::BillboardBlend::Alpha;
                bb.Quad(b);
            }
            if (mFont.Valid())
                bb.Text(base + glm::vec3(0.f, -1.0f, 0.f),
                        "Fixed", mFont, 0.18f,
                        { 0.3f, 0.9f, 1.f, 1.f }, 1.5f,
                        { 0.f, 0.f, 0.f, 1.f },
                        fra::BillboardAlign::Screen,
                        fra::BillboardLayer::Vfx);
        }

        // [5] Planar — flat decals stacked horizontally on the ground
        {
            const glm::vec3 base { 0.5f, 0.01f, 6.f };
            for (int r = 0; r < 3; ++r)
            {
                const float radius = 0.3f + r * 0.22f;
                fra::Billboard b;
                b.worldPos = base;
                b.size     = { radius, radius };
                b.color    = { 1.f, 0.85f, 0.1f, 0.75f - r * 0.18f };
                b.align    = fra::BillboardAlign::Planar;
                b.axisUp   = { 0.f, 1.f, 0.f };
                b.blend    = fra::BillboardBlend::Alpha;
                bb.Quad(b);
            }
            if (mFont.Valid())
                bb.Text(base + glm::vec3(0.f, 1.8f, 0.f),
                        "Planar", mFont, 0.18f,
                        { 1.f, 0.9f, 0.3f, 1.f }, 1.5f,
                        { 0.f, 0.f, 0.f, 1.f },
                        fra::BillboardAlign::Screen,
                        fra::BillboardLayer::Vfx);
        }

        // [6] ScreenSize — constant-pixel waypoint icons at varied depths
        {
            const glm::vec3 waypoints[] = {
                { 1.5f, 2.0f, 6.f },
                { 1.5f, 1.5f, 5.f },
                { 1.5f, 2.5f, 7.f },
            };
            for (const auto& wp : waypoints)
            {
                fra::Billboard b;
                b.worldPos        = wp;
                b.size            = { 0.04f, 0.04f };
                b.color           = { 1.f, 0.3f, 0.3f, 0.98f };
                b.align           = fra::BillboardAlign::Screen;
                b.screenSpaceSize = true;
                b.blend           = fra::BillboardBlend::Additive;
                bb.Quad(b);
            }
            if (mFont.Valid())
                bb.Text({ 1.5f, 0.8f, 6.f },
                        "ScrSz", mFont, 0.18f,
                        { 1.f, 0.4f, 0.4f, 1.f }, 1.5f,
                        { 0.f, 0.f, 0.f, 1.f },
                        fra::BillboardAlign::Screen,
                        fra::BillboardLayer::Vfx);
        }

        // [7] VelocityStretch — orbiting sparks
        {
            constexpr float kOrbitR = 0.45f;
            constexpr float kSpeed  = 2.2f;
            const glm::vec3 center { 2.5f, 1.5f, 6.f };
            for (int i = 0; i < 8; ++i)
            {
                const float a   = mOrbitAngles[i];
                const glm::vec3 pos {
                    center.x + kOrbitR * std::cos(a),
                    center.y,
                    center.z + kOrbitR * std::sin(a),
                };
                const glm::vec3 vel {
                    -kOrbitR * kSpeed * std::sin(a),
                    0.f,
                    kOrbitR * kSpeed * std::cos(a),
                };
                fra::Billboard b;
                b.worldPos = pos;
                b.size     = { 0.055f, 0.1f };
                b.color    = {
                    1.f, 0.5f + 0.5f * std::abs(std::sin(a)),
                    0.1f, 0.95f
                };
                b.align                = fra::BillboardAlign::Screen;
                b.velocityStretch      = true;
                b.velocity             = vel;
                b.velocityStretchScale = 0.5f;
                b.blend                = fra::BillboardBlend::Additive;
                bb.Quad(b);

                mOrbitAngles[i] += kSpeed * dt;
            }
            if (mFont.Valid())
                bb.Text(center + glm::vec3(0.f, -1.0f, 0.f),
                        "VelStr", mFont, 0.18f,
                        { 1.f, 0.75f, 0.2f, 1.f }, 1.5f,
                        { 0.f, 0.f, 0.f, 1.f },
                        fra::BillboardAlign::Screen,
                        fra::BillboardLayer::Vfx);
        }

        // [8] SoftParticle — depth-fade near the ground
        //     softFadeRange is in view-space depth (not NDC) — use a
        //     world-scale value (0.4 m) so the fade is visible.
        {
            const glm::vec3 base { 3.5f, 0.f, 6.f };
            for (int i = 0; i < 6; ++i)
            {
                const float a = i * (2.f * std::numbers::pi_v<float> / 6.f) +
                                mTime * 0.4f;
                const float bob =
                    0.1f + 0.3f * std::abs(std::sin(mTime * 0.7f + i));
                fra::Billboard b;
                b.worldPos      = base + glm::vec3(
                    0.35f * std::cos(a), bob, 0.35f * std::sin(a));
                b.size          = { 0.55f, 0.55f };
                b.color         = { 0.55f, 0.25f, 1.f, 0.88f };
                b.align         = fra::BillboardAlign::Screen;
                b.softParticle  = true;
                b.softFadeRange = 0.005f;
                b.blend         = fra::BillboardBlend::Alpha;
                bb.Quad(b);
            }
            if (mFont.Valid())
                bb.Text(base + glm::vec3(0.f, 1.8f, 0.f),
                        "Soft", mFont, 0.18f,
                        { 0.7f, 0.4f, 1.f, 1.f }, 1.5f,
                        { 0.f, 0.f, 0.f, 1.f },
                        fra::BillboardAlign::Screen,
                        fra::BillboardLayer::Vfx);
        }

        // ── [9 / 10] Characters + nameplates ──────────────────────────────
        const glm::vec3 playerPos { -2.f, 0.f, 0.f };
        const glm::vec3 enemyPos  {  2.f, 0.f, 0.f };

        // Player sprite — Dark Knight (blue)
        {
            fra::Billboard b;
            b.worldPos = playerPos + glm::vec3(0.f, 0.45f, 0.f);
            b.size     = { 0.38f, 0.88f };
            b.color    = { 0.15f, 0.22f, 0.75f, 0.95f };
            b.align    = fra::BillboardAlign::Cylindrical;
            b.blend    = fra::BillboardBlend::Alpha;
            bb.Quad(b);
        }
        {
            fra::Billboard ring;
            ring.worldPos = playerPos + glm::vec3(0.f, 0.005f, 0.f);
            ring.size     = { 0.3f, 0.3f };
            ring.color    = { 0.3f, 0.65f, 1.f, 0.55f };
            ring.align    = fra::BillboardAlign::Planar;
            ring.axisUp   = { 0.f, 1.f, 0.f };
            ring.blend    = fra::BillboardBlend::Alpha;
            bb.Quad(ring);
        }

        // Enemy sprite — Balgass boss (crimson, larger)
        {
            fra::Billboard b;
            b.worldPos = enemyPos + glm::vec3(0.f, 0.52f, 0.f);
            b.size     = { 0.46f, 1.0f };
            b.color    = { 0.55f, 0.04f, 0.08f, 0.95f };
            b.align    = fra::BillboardAlign::Cylindrical;
            b.blend    = fra::BillboardBlend::Alpha;
            bb.Quad(b);
        }
        {
            fra::Billboard ring;
            ring.worldPos = enemyPos + glm::vec3(0.f, 0.005f, 0.f);
            ring.size     = { 0.38f, 0.38f };
            ring.color    = { 1.f, 0.12f, 0.1f, 0.5f };
            ring.align    = fra::BillboardAlign::Planar;
            ring.axisUp   = { 0.f, 1.f, 0.f };
            ring.blend    = fra::BillboardBlend::Alpha;
            bb.Quad(ring);
        }

        // HP bars (Ui layer — rendered post-bloom)
        const float playerHp = 0.62f + 0.15f * std::sin(mTime * 0.65f);
        bb.HealthBar(
            playerPos + glm::vec3(0.f, 2.1f, 0.f),
            0.84f, 0.075f, playerHp,
            { 0.15f, 0.f, 0.f, 0.88f },
            { 0.12f, 0.78f, 0.12f, 0.95f });
        bb.HealthBar(
            enemyPos + glm::vec3(0.f, 2.38f, 0.f),
            0.92f, 0.075f, mEnemyHp,
            { 0.15f, 0.f, 0.f, 0.88f },
            { 0.78f, 0.1f, 0.1f, 0.95f });

        if (mFont.Valid())
        {
            // Player nameplate (Vfx layer = world-space, depth-tested)
            bb.Text(
                playerPos + glm::vec3(0.f, 2.46f, 0.f),
                "DarkKnight", mFont, 0.21f,
                { 0.65f, 0.82f, 1.f, 1.f }, 2.5f,
                { 0.f, 0.f, 0.f, 1.f },
                fra::BillboardAlign::Screen,
                fra::BillboardLayer::Vfx);
            bb.Text(
                playerPos + glm::vec3(0.f, 2.22f, 0.f),
                "[LV 399]", mFont, 0.13f,
                { 0.78f, 0.78f, 0.78f, 0.88f }, 1.5f,
                { 0.f, 0.f, 0.f, 1.f },
                fra::BillboardAlign::Screen,
                fra::BillboardLayer::Vfx);

            // Enemy nameplate (boss name pulses)
            const float bossA = 0.7f + 0.3f * std::sin(mTime * 2.f);
            bb.Text(
                enemyPos + glm::vec3(0.f, 2.8f, 0.f),
                "Balgass", mFont, 0.26f,
                { 1.f, 0.28f, 0.28f, bossA }, 2.5f,
                { 0.f, 0.f, 0.f, 1.f },
                fra::BillboardAlign::Screen,
                fra::BillboardLayer::Vfx);
            bb.Text(
                enemyPos + glm::vec3(0.f, 2.48f, 0.f),
                "[ BOSS ]", mFont, 0.13f,
                { 0.95f, 0.62f, 0.1f, 0.9f }, 1.5f,
                { 0.f, 0.f, 0.f, 1.f },
                fra::BillboardAlign::Screen,
                fra::BillboardLayer::Vfx);
        }

        // ── [11] Damage numbers ───────────────────────────────────────────
        mDmgTimer -= dt;
        if (mDmgTimer <= 0.f)
        {
            const int seq = static_cast<int>(mHitSeq % 7u);
            ++mHitSeq;
            if (seq == 6)
            {
                spawnDamage(playerPos + glm::vec3(0.f, 0.5f, 0.f),
                            DmgType::Heal);
            }
            else
            {
                DmgType t = DmgType::Normal;
                if (seq == 2)
                    t = DmgType::Critical;
                else if (seq == 3)
                    t = DmgType::Skill;
                else if (seq == 5)
                    t = DmgType::Miss;
                spawnDamage(enemyPos + glm::vec3(0.f, 0.5f, 0.f), t);
            }
            mDmgTimer = 0.42f + 0.38f * mDist01(mDmgRng);
        }

        if (mFont.Valid())
        {
            for (auto& d : mDamageNumbers)
            {
                if (!d.active)
                    continue;
                d.age += dt;
                if (d.age >= d.maxAge)
                {
                    d.active = false;
                    continue;
                }
                d.pos += d.vel * dt;
                d.vel.y = std::max(d.vel.y - 1.2f * dt, 0.08f);

                const float pop = (d.age < 0.12f)
                                      ? 1.5f - 0.5f * (d.age / 0.12f)
                                      : 1.f;
                const float fadeStart = d.maxAge * 0.55f;
                const float alpha =
                    (d.age > fadeStart)
                        ? 1.f - (d.age - fadeStart) /
                                    (d.maxAge - fadeStart)
                        : 1.f;

                glm::vec4 c  = d.color;
                glm::vec4 oc = d.outlineColor;
                c.a *= alpha;
                oc.a *= alpha;

                bb.Text(d.pos, d.text, mFont,
                        d.heightM * pop, c, d.outlineW, oc,
                        fra::BillboardAlign::Screen,
                        fra::BillboardLayer::Vfx);
            }
        }

        const float cpuMs = dt * 1000.f;
        mOverlay.Draw(*mRenderer, *mFreyaOptions, cpuMs,
                      mOverlay.ElapsedUpdateMs(), mLightService.get());
        mOverlay.EndFrame(*mRenderer);
    }

  private:
    struct DamageNumber
    {
        glm::vec3   pos {};
        glm::vec3   vel {};
        float       age       = 0.f;
        float       maxAge    = 1.25f;
        std::string text;
        glm::vec4   color { 1.f };
        glm::vec4   outlineColor { 0.f, 0.f, 0.f, 1.f };
        float       heightM  = 0.18f;
        float       outlineW = 2.0f;
        bool        active   = false;
    };

    void spawnDamage(const glm::vec3& basePos, const DmgType type)
    {
        DamageNumber* slot = nullptr;
        for (auto& d : mDamageNumbers)
            if (!d.active) { slot = &d; break; }
        if (!slot)
            return;

        auto& d  = *slot;
        d.active = true;
        d.age    = 0.f;
        d.maxAge = 1.25f;

        const float xOff = (mDist01(mDmgRng) - 0.5f) * 0.55f;
        d.pos = basePos + glm::vec3(xOff, 0.f, 0.f);
        d.vel = { xOff * 0.5f, 1.6f, 0.f };

        switch (type)
        {
            case DmgType::Normal:
                d.text = "-" + std::to_string(
                    200 + static_cast<int>(mDist01(mDmgRng) * 600.f));
                d.color        = { 1.f, 1.f, 1.f, 1.f };
                d.outlineColor = { 0.f, 0.f, 0.f, 1.f };
                d.heightM      = 0.18f;
                d.outlineW     = 2.0f;
                mEnemyHp = std::max(0.f, mEnemyHp - 0.06f);
                break;

            case DmgType::Critical:
                d.text = "!! " + std::to_string(
                    900 + static_cast<int>(mDist01(mDmgRng) * 1300.f));
                d.color        = { 1.f, 0.85f, 0.1f, 1.f };
                d.outlineColor = { 0.45f, 0.1f, 0.f, 1.f };
                d.heightM      = 0.28f;
                d.outlineW     = 3.0f;
                d.maxAge       = 1.5f;
                mEnemyHp = std::max(0.f, mEnemyHp - 0.14f);
                break;

            case DmgType::Skill:
                d.text = "-" + std::to_string(
                    450 + static_cast<int>(mDist01(mDmgRng) * 800.f));
                d.color        = { 1.f, 0.45f, 0.12f, 1.f };
                d.outlineColor = { 0.2f, 0.f, 0.f, 1.f };
                d.heightM      = 0.22f;
                d.outlineW     = 2.5f;
                mEnemyHp = std::max(0.f, mEnemyHp - 0.10f);
                break;

            case DmgType::Miss:
                d.text         = "MISS";
                d.color        = { 0.8f, 0.8f, 0.8f, 1.f };
                d.outlineColor = { 0.1f, 0.1f, 0.1f, 1.f };
                d.heightM      = 0.15f;
                d.outlineW     = 1.5f;
                break;

            case DmgType::Heal:
                d.text = "+" + std::to_string(
                    100 + static_cast<int>(mDist01(mDmgRng) * 350.f));
                d.color        = { 0.25f, 1.f, 0.42f, 1.f };
                d.outlineColor = { 0.f, 0.18f, 0.f, 1.f };
                d.heightM      = 0.18f;
                d.outlineW     = 2.0f;
                break;
        }

        if (mEnemyHp <= 0.f)
            mEnemyHp = 1.f;
    }

    // ── Services ──────────────────────────────────────────────────────────────
    fra::Ref<fra::MeshPool>     mMeshPool;
    fra::Ref<fra::TexturePool>  mTexturePool;
    fra::Ref<fra::MaterialPool> mMaterialPool;
    fra::Ref<fra::LightService> mLightService;
    fra::Ref<fra::FreyaOptions> mFreyaOptions;

    fra::MeshHandle     mGroundMesh {};
    fra::MaterialHandle mGroundMat {};
    fra::Scene          mScene;

    std::vector<float> mOrbitAngles;

    fra::FontAtlas mFont;
    float          mEnemyHp  = 1.f;
    float          mDmgTimer = 0.7f;
    std::uint32_t  mHitSeq   = 0;

    static constexpr int                        kMaxDmg = 24;
    std::array<DamageNumber, kMaxDmg>           mDamageNumbers {};
    std::mt19937                                mDmgRng { 0xC0DEu };
    std::uniform_real_distribution<float>       mDist01 { 0.f, 1.f };

    float mTime = 0.f;

    FreyaExamples::FlyCam       mCam;
    FreyaExamples::DebugOverlay mOverlay;
};

int main(int, const char**)
{
    return fra::RunApp<MainApp>(
        [](fra::FreyaOptionsBuilder& o) {
            o.SetTitle("BillboardShowcase")
                .SetWidth(1600)
                .SetHeight(900)
                .SetVSync(true)
                .WithReverseZ()
                .SetIblIntensity(0.f)
                .SetEnvironmentMapPath("")
                .SetBloomQuality(fra::BloomQuality::Ultra);
        },
        [](skr::LoggingExtension& l) {
            FreyaExamples::ConfigureLogging(l, "BillboardShowcase.log");
        });
}
