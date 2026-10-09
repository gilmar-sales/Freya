#include <Freya/Freya.hpp>

#include <FreyaExamples/DebugOverlay.hpp>
#include <FreyaExamples/ExampleLogging.hpp>
#include <FreyaExamples/FlyCam.hpp>
#include <FreyaExamples/GpuTimingCapture.hpp>
#include <FreyaExamples/GroundMesh.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

// ── Light Stress Showcase ────────────────────────────────────────────────────
//
// A deliberately dense analytical-light scene for profiling deferred and
// tiled lighting.  The 8x8 point-light field is animated every frame, while
// the low-poly blocks make the per-pixel lighting cost easy to see.
//
// Controls: RMB + WASD fly | Space toggles light animation
//
// Baseline capture (headless, prints per-stage GPU ms and exits):
//   FREYA_BENCH_FRAMES=600 FREYA_BENCH_OUT=bench.csv ./LightStressShowcase
// Scene knobs: FREYA_STRESS_GRID=1..8 (grid x grid point lights, default 8)
//              FREYA_STRESS_SHADOWS=1 (enable the shadow pass)
// See GpuTimingCapture.hpp for the FREYA_BENCH_* variables.

namespace
{
    fra::MeshHandle BuildBox(fra::MeshPool& pool, const glm::vec3 minCorner,
                             const glm::vec3 maxCorner)
    {
        const std::array faces {
            std::array { glm::vec3 { minCorner.x, maxCorner.y, minCorner.z },
                         glm::vec3 { minCorner.x, maxCorner.y, maxCorner.z },
                         glm::vec3 { maxCorner.x, maxCorner.y, maxCorner.z },
                         glm::vec3 { maxCorner.x, maxCorner.y, minCorner.z },
                         glm::vec3 { 0.f, 1.f, 0.f } },
            std::array { glm::vec3 { minCorner.x, minCorner.y, maxCorner.z },
                         glm::vec3 { minCorner.x, minCorner.y, minCorner.z },
                         glm::vec3 { maxCorner.x, minCorner.y, minCorner.z },
                         glm::vec3 { maxCorner.x, minCorner.y, maxCorner.z },
                         glm::vec3 { 0.f, -1.f, 0.f } },
            std::array { glm::vec3 { minCorner.x, minCorner.y, maxCorner.z },
                         glm::vec3 { maxCorner.x, minCorner.y, maxCorner.z },
                         glm::vec3 { maxCorner.x, maxCorner.y, maxCorner.z },
                         glm::vec3 { minCorner.x, maxCorner.y, maxCorner.z },
                         glm::vec3 { 0.f, 0.f, 1.f } },
            std::array { glm::vec3 { maxCorner.x, minCorner.y, minCorner.z },
                         glm::vec3 { minCorner.x, minCorner.y, minCorner.z },
                         glm::vec3 { minCorner.x, maxCorner.y, minCorner.z },
                         glm::vec3 { maxCorner.x, maxCorner.y, minCorner.z },
                         glm::vec3 { 0.f, 0.f, -1.f } },
            std::array { glm::vec3 { maxCorner.x, minCorner.y, minCorner.z },
                         glm::vec3 { maxCorner.x, maxCorner.y, minCorner.z },
                         glm::vec3 { maxCorner.x, maxCorner.y, maxCorner.z },
                         glm::vec3 { maxCorner.x, minCorner.y, maxCorner.z },
                         glm::vec3 { 1.f, 0.f, 0.f } },
            std::array { glm::vec3 { minCorner.x, minCorner.y, maxCorner.z },
                         glm::vec3 { minCorner.x, maxCorner.y, maxCorner.z },
                         glm::vec3 { minCorner.x, maxCorner.y, minCorner.z },
                         glm::vec3 { minCorner.x, minCorner.y, minCorner.z },
                         glm::vec3 { -1.f, 0.f, 0.f } },
        };

        std::vector<fra::Vertex>   vertices;
        std::vector<std::uint32_t> indices;
        vertices.reserve(24);
        indices.reserve(36);
        for (const auto& face : faces)
        {
            const auto base    = static_cast<std::uint32_t>(vertices.size());
            const auto tangent = glm::normalize(face[1] - face[0]);
            vertices.push_back(
                { face[0], glm::vec3(1.f), face[4], tangent, { 0.f, 0.f } });
            vertices.push_back(
                { face[1], glm::vec3(1.f), face[4], tangent, { 1.f, 0.f } });
            vertices.push_back(
                { face[2], glm::vec3(1.f), face[4], tangent, { 1.f, 1.f } });
            vertices.push_back(
                { face[3], glm::vec3(1.f), face[4], tangent, { 0.f, 1.f } });
            indices.insert(indices.end(), { base, base + 1, base + 2, base,
                                            base + 2, base + 3 });
        }
        return pool.CreateMesh(vertices, indices);
    }

    int StressGrid()
    {
        const char* value = std::getenv("FREYA_STRESS_GRID");
        return value != nullptr
                   ? std::clamp(
                         static_cast<int>(std::strtol(value, nullptr, 10)), 1,
                         8)
                   : 8;
    }

    bool StressShadows()
    {
        const char* value = std::getenv("FREYA_STRESS_SHADOWS");
        return value != nullptr && *value == '1';
    }
} // namespace

class MainApp final : public fra::AbstractApplication
{
  public:
    explicit MainApp(const fra::Ref<fra::ServiceProvider>& sp) :
        AbstractApplication(sp)
    {
        auto ws       = GetMainServiceProvider();
        mMeshPool     = sp->GetService<fra::MeshPool>();
        mMaterialPool = sp->GetService<fra::MaterialPool>();
        mLightService = ws->GetService<fra::LightService>();
        mFreyaOptions = ws->GetService<fra::FreyaOptions>();
    }

    void StartUp() override
    {
        mCam.window     = mWindow;
        mCam.cameraPos  = { 0.f, 13.f, 24.f };
        mCam.yaw        = -90.f;
        mCam.pitch      = -25.f;
        mCam.moveSpeed  = 12.f;
        mCam.blockMouse = [this] { return mOverlay.WantsCaptureMouse(); };
        mCam.BindInput(*mEventManager);

        if (mPlatform)
            mOverlay.Init(*mRenderer, *mWindow, GetMainServiceProvider());

        mEventManager->Subscribe<fra::KeyReleasedEvent>(
            [this](const fra::KeyReleasedEvent& event) {
                if (event.key == fra::KeyCode::Space)
                    mAnimateLights = !mAnimateLights;
            });

        const auto ground = FreyaExamples::CreateGroundPlane(
            *mMeshPool, 44.f, glm::vec3(0.22f));
        const auto groundMaterial = mMaterialPool->Create({
            .albedoFactor    = { 0.25f, 0.27f, 0.30f, 1.f },
            .roughnessFactor = 0.82f,
        });
        AddInstance(ground, groundMaterial, glm::mat4(1.f), 1u);

        const auto block =
            BuildBox(*mMeshPool, { -0.8f, 0.f, -0.8f }, { 0.8f, 1.8f, 0.8f });
        const auto    blockMaterial = mMaterialPool->Create({
            .albedoFactor    = { 0.48f, 0.50f, 0.54f, 1.f },
            .roughnessFactor = 0.68f,
        });
        std::uint32_t entityId      = 10u;
        for (int z = -3; z <= 3; ++z)
            for (int x = -3; x <= 3; ++x)
            {
                glm::mat4 transform = glm::translate(
                    glm::mat4(1.f), glm::vec3(x * 4.5f, 0.f, z * 4.5f));
                transform = glm::scale(
                    transform,
                    glm::vec3(1.f, 0.7f + 0.1f * ((x + z + 6) % 4), 1.f));
                AddInstance(block, blockMaterial, transform, entityId++);
            }

        mLightService->AddLight(fra::MakeDirectionalLight(
            { -0.3f, -1.f, -0.2f }, { 0.28f, 0.31f, 0.38f }, 0.25f));

        const int       kGrid    = StressGrid();
        constexpr float kSpacing = 4.5f;
        const float     kHalf    = static_cast<float>(kGrid - 1) * 0.5f;
        const float     kSpan    = static_cast<float>(std::max(kGrid - 1, 1));
        mLightHandles.reserve(kGrid * kGrid);
        mBasePositions.reserve(kGrid * kGrid);
        for (int z = 0; z < kGrid; ++z)
            for (int x = 0; x < kGrid; ++x)
            {
                const glm::vec3 position { (x - kHalf) * kSpacing, 3.0f,
                                           (z - kHalf) * kSpacing };
                const glm::vec3 color {
                    0.35f + 0.65f * static_cast<float>(x) / kSpan,
                    0.25f + 0.70f * static_cast<float>(z) / kSpan,
                    1.0f - 0.55f * static_cast<float>(x + z) / (2.f * kSpan),
                };
                mLightHandles.push_back(mLightService->AddLight(
                    fra::MakePointLight(position, color, 5.f, 7.f)));
                mBasePositions.push_back(position);
            }

        if (mCapture.Active())
            mAnimateLights = false;

        mFreyaOptions->title = "LightStressShowcase";
        std::cout << "LightStressShowcase\n"
                     "  animated point lights (FREYA_STRESS_GRID)\n"
                     "  SPACE toggles light animation\n"
                     "  RMB + WASD to navigate\n";
    }

    void Update() override
    {
        if (mCapture.Tick(*mRenderer,
                          "lights=" + std::to_string(mLightHandles.size()) +
                              " shadows=" + (StressShadows() ? "on" : "off")))
        {
            mWindow->Close();
            return;
        }
        mOverlay.MarkUpdateStart();
        mOverlay.BeginFrame();
        const float dt = mWindow->GetDeltaTime();
        mTime += dt;
        mCam.Update(dt);

        if (mAnimateLights)
            for (std::size_t i = 0; i < mLightHandles.size(); ++i)
            {
                const auto* current = mLightService->GetLight(mLightHandles[i]);
                if (!current)
                    continue;
                auto        light = *current;
                const float phase =
                    mTime * 1.2f + static_cast<float>(i) * 0.37f;
                light.position  = mBasePositions[i] +
                                  glm::vec3(0.f, std::sin(phase) * 1.2f, 0.f);
                light.intensity = 6.0f + std::sin(phase * 1.7f) * 1.5f;
                mLightService->UpdateLight(mLightHandles[i], light);
            }

        mRenderer->BeginFrame();
        mCam.Apply(*mRenderer);
        mScene.Upload(*mRenderer);
        mOverlay.Draw(*mRenderer, *mFreyaOptions, dt * 1000.f,
                      mOverlay.ElapsedUpdateMs(), mLightService.get());
        mOverlay.EndFrame(*mRenderer);
    }

  private:
    void AddInstance(const fra::MeshHandle     mesh,
                     const fra::MaterialHandle material,
                     const glm::mat4& transform, const std::uint32_t entityId)
    {
        fra::Scene::Instance instance {};
        instance.transform = fra::SceneTransform::FromMatrix(transform);
        instance.mesh      = mesh;
        instance.material  = material;
        instance.entityId  = entityId;
        instance.mobility  = fra::Mobility::Static;
        mScene.Add(instance);
    }

    fra::Ref<fra::MeshPool>         mMeshPool;
    fra::Ref<fra::MaterialPool>     mMaterialPool;
    fra::Ref<fra::LightService>     mLightService;
    fra::Ref<fra::FreyaOptions>     mFreyaOptions;
    fra::Scene                      mScene;
    FreyaExamples::FlyCam           mCam;
    FreyaExamples::DebugOverlay     mOverlay;
    FreyaExamples::GpuTimingCapture mCapture;
    std::vector<fra::LightHandle>   mLightHandles;
    std::vector<glm::vec3>          mBasePositions;
    float                           mTime          = 0.f;
    bool                            mAnimateLights = true;
};

int main(int, const char**)
{
    return fra::RunApp<MainApp>(
        [](fra::FreyaOptionsBuilder& options) {
            options.SetTitle("Light Stress Showcase")
                .SetWidth(1920)
                .SetHeight(1080)
                .SetVSync(false)
                .SetSampleCount(1)
                .SetMaxLights(64)
                .SetEnableShadows(StressShadows())
                .SetEnableBloom(false)
                .SetIblIntensity(0.04f)
                .SetEnvironmentMapPath(
                    "./Resources/Environments/studio_small_09_4k.hdr");
        },
        [](skr::LoggingExtension& logging) {
            FreyaExamples::ConfigureLogging(logging, "LightStressShowcase.log");
        });
}
