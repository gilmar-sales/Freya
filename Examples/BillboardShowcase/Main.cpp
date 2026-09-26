#include <Freya/Freya.hpp>

#include <FreyaExamples/DebugOverlay.hpp>
#include <FreyaExamples/ExampleLogging.hpp>
#include <FreyaExamples/FlyCam.hpp>
#include <FreyaExamples/GroundMesh.hpp>

#include <cmath>
#include <iostream>
#include <numbers>
#include <vector>

// ── Billboard Showcase ────────────────────────────────────────────────────────
//
// Demonstrates every billboard alignment mode and render feature.
//
// Row 1 (z = -4):
//   [1] Screen     x=-6  — standard view-plane quad (baseline)
//   [2] Cylindrical x=-2  — yaw-only rotation, always upright
//   [3] Spherical  x=+2  — per-instance point-toward-camera
//   [4] FixedAxis  x=+6  — cylindrical with a custom axis (Z-axis)
//
// Row 2 (z = +4):
//   [5] Planar      x=-6  — flat on a surface (axisUp = ground normal)
//   [6] ScreenSize  x=-2  — constant-pixel waypoint icons
//   [7] VelocityStretch x=+2  — sparks stretched along orbit velocity
//   [8] SoftParticle    x=+6  — depth-fade where they meet the ground
//
// Controls: RMB look, WASD move, Q/E up/down

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
        mCam.cameraPos  = { 0.f, 3.f, 14.f };
        mCam.yaw        = -90.f;
        mCam.pitch      = -12.f;
        mCam.moveSpeed  = 6.f;
        mCam.blockMouse = [this] { return mOverlay.WantsCaptureMouse(); };
        mCam.BindInput(*mEventManager);

        if (mPlatform)
            mOverlay.Init(*mRenderer, *mWindow, *mPlatform);

        mGroundMesh = FreyaExamples::CreateGroundPlane(
            *mMeshPool, 24.f, glm::vec3(0.15f));
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

        // Evenly space 8 sparks around the orbit circle for VelocityStretch
        mOrbitAngles.resize(8);
        for (int i = 0; i < 8; ++i)
            mOrbitAngles[i] =
                i * (2.f * std::numbers::pi_v<float> / 8.f);

        std::cout
            << "BillboardShowcase\n"
               "  Row 1  (z = -4)\n"
               "  [1] Screen     — standard view-plane quad\n"
               "  [2] Cylindrical — yaw-only, always upright\n"
               "  [3] Spherical  — per-instance point-toward-camera\n"
               "  [4] FixedAxis  — cylindrical with a custom up axis\n"
               "  Row 2  (z = +4)\n"
               "  [5] Planar      — flat on surface (axisUp = normal)\n"
               "  [6] ScreenSize  — constant NDC-size waypoints\n"
               "  [7] VelocityStretch — sparks stretched along orbit\n"
               "  [8] SoftParticle    — depth-fade near the ground\n"
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

        // ── [1] Screen (view-plane) ────────────────────────────────────────
        // Three blue stacked quads — they simply face the camera, all axes.
        {
            const glm::vec3 base { -6.f, 1.5f, -4.f };
            for (int i = -1; i <= 1; ++i)
            {
                fra::Billboard b;
                b.worldPos = base + glm::vec3(0.f, i * 0.6f, 0.f);
                b.size     = { 0.45f, 0.45f };
                b.color    = { 0.3f, 0.55f, 1.f, 0.92f };
                b.align    = fra::BillboardAlign::Screen;
                b.blend    = fra::BillboardBlend::Alpha;
                bb.Quad(b);
            }
        }

        // ── [2] Cylindrical ───────────────────────────────────────────────
        // Three green stacked quads — yaw-only, pitch is locked to world up.
        {
            const glm::vec3 base { -2.f, 1.5f, -4.f };
            for (int i = -1; i <= 1; ++i)
            {
                fra::Billboard b;
                b.worldPos = base + glm::vec3(0.f, i * 0.6f, 0.f);
                b.size     = { 0.45f, 0.45f };
                b.color    = { 0.25f, 1.f, 0.45f, 0.92f };
                b.align    = fra::BillboardAlign::Cylindrical;
                b.blend    = fra::BillboardBlend::Alpha;
                bb.Quad(b);
            }
        }

        // ── [3] Spherical ─────────────────────────────────────────────────
        // Magenta quads — point-toward-camera, correct even at oblique angles.
        {
            const glm::vec3 base { 2.f, 1.5f, -4.f };
            for (int i = -1; i <= 1; ++i)
            {
                fra::Billboard b;
                b.worldPos = base + glm::vec3(0.f, i * 0.6f, 0.f);
                b.size     = { 0.45f, 0.45f };
                b.color    = { 1.f, 0.35f, 0.9f, 0.92f };
                b.align    = fra::BillboardAlign::Spherical;
                b.blend    = fra::BillboardBlend::Alpha;
                bb.Quad(b);
            }
        }

        // ── [4] FixedAxis (custom up = Z) ─────────────────────────────────
        // Cyan quads with axisUp = (0,0,1). They rotate around the Z world
        // axis instead of Y, appearing to lean as the camera moves vertically.
        {
            const glm::vec3 base { 6.f, 1.5f, -4.f };
            for (int i = -1; i <= 1; ++i)
            {
                fra::Billboard b;
                b.worldPos = base + glm::vec3(i * 0.6f, 0.f, 0.f);
                b.size     = { 0.45f, 0.45f };
                b.color    = { 0.2f, 0.9f, 1.f, 0.92f };
                b.align    = fra::BillboardAlign::FixedAxis;
                b.axisUp   = { 0.f, 0.f, 1.f };
                b.blend    = fra::BillboardBlend::Alpha;
                bb.Quad(b);
            }
        }

        // ── [5] Planar (flat decal on ground) ─────────────────────────────
        // Three concentric yellow rings lying flat on the ground (axisUp = Y).
        {
            const glm::vec3 base { -6.f, 0.01f, 4.f };
            for (int r = 0; r < 3; ++r)
            {
                const float radius = 0.28f + r * 0.24f;
                fra::Billboard b;
                b.worldPos = base;
                b.size     = { radius, radius };
                b.color    = { 1.f, 0.85f, 0.1f, 0.7f - r * 0.18f };
                b.align    = fra::BillboardAlign::Planar;
                b.axisUp   = { 0.f, 1.f, 0.f };
                b.blend    = fra::BillboardBlend::Alpha;
                bb.Quad(b);
            }
        }

        // ── [6] ScreenSize (constant-pixel waypoints) ─────────────────────
        // Three red icons at different world depths. They all stay the same
        // apparent size because screenSpaceSize = true.
        {
            const glm::vec3 waypoints[] = {
                { -2.f, 1.0f, 1.f },
                { -2.5f, 2.5f, 4.f },
                { -1.5f, 0.8f, 7.f },
            };
            for (const auto& wp : waypoints)
            {
                fra::Billboard b;
                b.worldPos        = wp;
                b.size            = { 0.025f, 0.025f }; // NDC half-extents
                b.color           = { 1.f, 0.3f, 0.3f, 0.95f };
                b.align           = fra::BillboardAlign::Screen;
                b.screenSpaceSize = true;
                b.blend           = fra::BillboardBlend::Alpha;
                bb.Quad(b);
            }
        }

        // ── [7] VelocityStretch (orbiting sparks) ─────────────────────────
        // Orange sparks orbit a central point. Each one is stretched along its
        // instantaneous tangent velocity, creating comet-like tails.
        {
            constexpr float kOrbitR = 0.75f;
            constexpr float kSpeed  = 1.8f;
            const glm::vec3 center { 2.f, 1.2f, 4.f };
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
                b.worldPos             = pos;
                b.size                 = { 0.06f, 0.11f };
                b.color = { 1.f, 0.55f + 0.45f * std::abs(std::sin(a)), 0.15f,
                            0.9f };
                b.align                = fra::BillboardAlign::Screen;
                b.velocityStretch      = true;
                b.velocity             = vel;
                b.velocityStretchScale = 0.55f;
                b.blend                = fra::BillboardBlend::Additive;
                bb.Quad(b);

                mOrbitAngles[i] += kSpeed * dt;
            }
        }

        // ── [8] SoftParticle (depth-fade near ground) ─────────────────────
        // Purple translucent quads hover just above the ground. Without soft
        // particles you see hard intersection edges; with softParticle = true
        // they fade out where they meet the ground mesh.
        {
            const glm::vec3 base { 6.f, 0.f, 4.f };
            for (int i = 0; i < 6; ++i)
            {
                const float a = i * (2.f * std::numbers::pi_v<float> / 6.f) +
                                mTime * 0.35f;
                const float bob = 0.05f +
                                  0.2f *
                                      std::abs(std::sin(mTime * 0.6f + i));
                fra::Billboard b;
                b.worldPos      = base +
                                  glm::vec3(0.4f * std::cos(a), bob,
                                            0.4f * std::sin(a));
                b.size          = { 0.5f, 0.5f };
                b.color         = { 0.55f, 0.25f, 1.f, 0.8f };
                b.align         = fra::BillboardAlign::Screen;
                b.softParticle  = true;
                b.softFadeRange = 0.006f;
                b.blend         = fra::BillboardBlend::Alpha;
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

    std::vector<float> mOrbitAngles;
    float              mTime = 0.f;

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
