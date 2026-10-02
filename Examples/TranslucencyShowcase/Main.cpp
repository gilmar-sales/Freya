#include <Freya/Freya.hpp>

#include <FreyaExamples/DebugOverlay.hpp>
#include <FreyaExamples/ExampleLogging.hpp>
#include <FreyaExamples/FlyCam.hpp>
#include <FreyaExamples/GroundMesh.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <vector>

namespace
{
    fra::MeshHandle BuildBox(fra::MeshPool& pool)
    {
        constexpr std::array faces {
            std::array { glm::vec3 { -1.f, -1.f, 1.f }, glm::vec3 { 1.f, -1.f, 1.f },
                         glm::vec3 { 1.f, 1.f, 1.f }, glm::vec3 { -1.f, 1.f, 1.f },
                         glm::vec3 { 0.f, 0.f, 1.f } },
            std::array { glm::vec3 { 1.f, -1.f, -1.f }, glm::vec3 { -1.f, -1.f, -1.f },
                         glm::vec3 { -1.f, 1.f, -1.f }, glm::vec3 { 1.f, 1.f, -1.f },
                         glm::vec3 { 0.f, 0.f, -1.f } },
            std::array { glm::vec3 { 1.f, -1.f, 1.f }, glm::vec3 { 1.f, -1.f, -1.f },
                         glm::vec3 { 1.f, 1.f, -1.f }, glm::vec3 { 1.f, 1.f, 1.f },
                         glm::vec3 { 1.f, 0.f, 0.f } },
            std::array { glm::vec3 { -1.f, -1.f, -1.f }, glm::vec3 { -1.f, -1.f, 1.f },
                         glm::vec3 { -1.f, 1.f, 1.f }, glm::vec3 { -1.f, 1.f, -1.f },
                         glm::vec3 { -1.f, 0.f, 0.f } },
            std::array { glm::vec3 { -1.f, 1.f, 1.f }, glm::vec3 { 1.f, 1.f, 1.f },
                         glm::vec3 { 1.f, 1.f, -1.f }, glm::vec3 { -1.f, 1.f, -1.f },
                         glm::vec3 { 0.f, 1.f, 0.f } },
            std::array { glm::vec3 { -1.f, -1.f, -1.f }, glm::vec3 { 1.f, -1.f, -1.f },
                         glm::vec3 { 1.f, -1.f, 1.f }, glm::vec3 { -1.f, -1.f, 1.f },
                         glm::vec3 { 0.f, -1.f, 0.f } },
        };

        std::vector<fra::Vertex> vertices;
        std::vector<std::uint32_t> indices;
        for (const auto& face : faces)
        {
            const auto base = static_cast<std::uint32_t>(vertices.size());
            const auto tangent = glm::normalize(face[1] - face[0]);
            vertices.push_back({ face[0], glm::vec3(1.f), face[4], tangent, { 0.f, 0.f } });
            vertices.push_back({ face[1], glm::vec3(1.f), face[4], tangent, { 1.f, 0.f } });
            vertices.push_back({ face[2], glm::vec3(1.f), face[4], tangent, { 1.f, 1.f } });
            vertices.push_back({ face[3], glm::vec3(1.f), face[4], tangent, { 0.f, 1.f } });
            indices.insert(indices.end(), { base, base + 1, base + 2,
                                            base, base + 2, base + 3 });
        }
        return pool.CreateMesh(vertices, indices);
    }
} // namespace

class MainApp final : public fra::AbstractApplication
{
  public:
    explicit MainApp(const fra::Ref<fra::ServiceProvider>& sp) :
        AbstractApplication(sp)
    {
        const auto ws = GetMainServiceProvider();
        mMeshPool = sp->GetService<fra::MeshPool>();
        mMaterialPool = sp->GetService<fra::MaterialPool>();
        mLightService = ws->GetService<fra::LightService>();
        mFreyaOptions = ws->GetService<fra::FreyaOptions>();
    }

    void StartUp() override
    {
        mCam.window = mWindow;
        mCam.cameraPos = { 0.f, 4.f, 13.f };
        mCam.yaw = -90.f;
        mCam.pitch = -12.f;
        mCam.blockMouse = [this] { return mOverlay.WantsCaptureMouse(); };
        mCam.BindInput(*mEventManager);
        if (mPlatform)
            mOverlay.Init(*mRenderer, *mWindow, *mPlatform);

        const auto floor = FreyaExamples::CreateGroundPlane(*mMeshPool, 26.f,
                                                             glm::vec3(0.2f));
        const auto floorMat = mMaterialPool->Create({
            .albedoFactor = { 0.28f, 0.30f, 0.34f, 1.f },
            .roughnessFactor = 0.7f,
        });
        AddInstance(floor, floorMat, glm::scale(glm::mat4(1.f), glm::vec3(1.f, 1.f, 1.f)), 1);

        const auto box = BuildBox(*mMeshPool);
        const auto opaque = mMaterialPool->Create({
            .albedoFactor = { 0.8f, 0.82f, 0.86f, 1.f },
            .roughnessFactor = 0.35f,
            .metalnessFactor = 0.15f,
        });
        for (int x = -2; x <= 2; ++x)
        {
            auto transform = glm::translate(glm::mat4(1.f), { x * 2.2f, 1.15f, -2.8f });
            transform = glm::scale(transform, { 0.72f, 1.15f, 0.72f });
            AddInstance(box, opaque, transform, static_cast<std::uint32_t>(10 + x + 2));
        }

        constexpr std::array colors {
            glm::vec3(0.15f, 0.75f, 1.f), glm::vec3(1.f, 0.25f, 0.12f),
            glm::vec3(0.30f, 1.f, 0.42f),
        };
        for (std::size_t i = 0; i < colors.size(); ++i)
        {
            const auto material = mMaterialPool->Create({
                .albedoFactor = { colors[i].r, colors[i].g, colors[i].b, 0.22f },
                .roughnessFactor = 0.08f,
                .metalnessFactor = 0.f,
                .alphaMode = fra::AlphaMode::Blend,
                .transmission = 0.88f,
                .ior = 1.46f,
            });
            auto transform = glm::translate(glm::mat4(1.f),
                                             { (static_cast<float>(i) - 1.f) * 3.1f,
                                               1.7f, 1.2f });
            transform = glm::scale(transform, { 0.95f, 1.7f, 0.12f });
            AddInstance(box, material, transform,
                        static_cast<std::uint32_t>(30 + i), true);
        }

        mLightService->AddLight(fra::MakeDirectionalLight(
            { -0.35f, -1.f, -0.25f }, { 0.55f, 0.58f, 0.68f }, 0.35f));
        const std::array positions { glm::vec3(-4.f, 3.f, 2.f),
                                     glm::vec3(0.f, 3.5f, 2.f),
                                     glm::vec3(4.f, 3.f, 2.f) };
        for (std::size_t i = 0; i < positions.size(); ++i)
        {
            mLights.push_back(mLightService->AddLight(
                fra::MakePointLight(positions[i], colors[i], 24.f, 10.f)));
            mLightPositions.push_back(positions[i]);
        }

        mFreyaOptions->title = "Translucency Showcase";
        std::cout << "Translucency Showcase\n"
                     "  Colored transmission panels with animated point lights\n"
                     "  RMB + WASD to navigate\n";
    }

    void Update() override
    {
        const float dt = mWindow->GetDeltaTime();
        mTime += dt;
        mCam.Update(dt);
        for (std::size_t i = 0; i < mLights.size(); ++i)
        {
            if (const auto* current = mLightService->GetLight(mLights[i]))
            {
                auto light = *current;
                light.position = mLightPositions[i] +
                    glm::vec3(0.f, std::sin(mTime * 1.5f + static_cast<float>(i)) * 0.55f, 0.f);
                mLightService->UpdateLight(mLights[i], light);
            }
        }
        mOverlay.MarkUpdateStart();
        mOverlay.BeginFrame();
        mRenderer->BeginFrame();
        mCam.Apply(*mRenderer);
        mScene.Upload(*mRenderer);
        mOverlay.Draw(*mRenderer, *mFreyaOptions, dt * 1000.f,
                      mOverlay.ElapsedUpdateMs(), mLightService.get());
        mOverlay.EndFrame(*mRenderer);
    }

  private:
    void AddInstance(fra::MeshHandle mesh, fra::MaterialHandle material,
                     const glm::mat4& transform, std::uint32_t id,
                     bool translucent = false)
    {
        fra::Scene::Instance instance {};
        instance.transform = fra::SceneTransform::FromMatrix(transform);
        instance.mesh = mesh;
        instance.material = material;
        instance.entityId = id;
        instance.mobility = fra::Mobility::Static;
        if (translucent)
            instance.flags |= fra::SceneInstanceFlags::Translucent;
        mScene.Add(instance);
    }

    fra::Ref<fra::MeshPool> mMeshPool;
    fra::Ref<fra::MaterialPool> mMaterialPool;
    fra::Ref<fra::LightService> mLightService;
    fra::Ref<fra::FreyaOptions> mFreyaOptions;
    fra::Scene mScene;
    FreyaExamples::FlyCam mCam;
    FreyaExamples::DebugOverlay mOverlay;
    std::vector<fra::LightHandle> mLights;
    std::vector<glm::vec3> mLightPositions;
    float mTime = 0.f;
};

int main(int, const char**)
{
    return fra::RunApp<MainApp>(
        [](fra::FreyaOptionsBuilder& options) {
            options.SetTitle("Translucency Showcase")
                .SetWidth(1600)
                .SetHeight(900)
                .SetVSync(false)
                .SetSampleCount(1)
                .SetMaxLights(8)
                .SetEnableShadows(false)
                .SetIblIntensity(0.08f)
                .SetEnvironmentMapPath("./Resources/Environments/studio_small_09_4k.hdr");
        },
        [](skr::LoggingExtension& logging) {
            FreyaExamples::ConfigureLogging(logging, "TranslucencyShowcase.log");
        });
}
