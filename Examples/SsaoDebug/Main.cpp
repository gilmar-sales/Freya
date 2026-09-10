#include <Freya/Freya.hpp>

#include <FreyaExamples/CullAabbDebugDraw.hpp>
#include <FreyaExamples/DebugOverlay.hpp>
#include <FreyaExamples/ExampleLogging.hpp>
#include <FreyaExamples/FlyCam.hpp>
#include <FreyaExamples/GroundMesh.hpp>

#include <iostream>
#include <vector>

class MainApp final : public fra::AbstractApplication
{
  public:
    explicit MainApp(const fra::Ref<fra::ServiceProvider>& serviceProvider) :
        AbstractApplication(serviceProvider)
    {
        auto windowServices = GetMainServiceProvider();
        mMeshPool           = serviceProvider->GetService<fra::MeshPool>();
        mMaterialPool       = serviceProvider->GetService<fra::MaterialPool>();
        mLightService       = windowServices->GetService<fra::LightService>();
        mFreyaOptions       = windowServices->GetService<fra::FreyaOptions>();
    }

    void StartUp() override
    {
        mCam.window     = mWindow;
        mCam.moveSpeed  = 10.0f;
        mCam.cameraPos  = { 0.2f, 1.4f, 4.8f };
        mCam.yaw        = -95.0f;
        mCam.pitch      = -12.0f;
        mCam.blockMouse = [this] { return mOverlay.WantsCaptureMouse(); };
        mCam.BindInput(*mEventManager);

        if (mPlatform)
            mOverlay.Init(*mRenderer, *mWindow, *mPlatform);
        mOverlay.SetCullDumpExampleName("SsaoDebug");

        mRenderer->ClearProjections();

        mGroundMesh = FreyaExamples::CreateGroundPlane(
            *mMeshPool, 20.0f, glm::vec3(0.72f, 0.72f, 0.76f));
        mGroundMaterial = mMaterialPool->Create({
            .albedoFactor    = { 0.45f, 0.45f, 0.48f, 1.0f },
            .roughnessFactor = 0.9f,
            .metalnessFactor = 0.0f,
        });

        mHelmetModel = mMeshPool->CreateModelFromFile(
            "./Resources/Models/DamagedHelmet.gltf");
        mDragonModel = mMeshPool->CreateModelFromFile(
            "./Resources/Models/DragonAttenuation.gltf");
        mShipModel =
            mMeshPool->CreateModelFromFile("./Resources/Models/ally_ship.glb");

        if (mHelmetModel.empty())
            std::cerr << "Failed to load DamagedHelmet.gltf\n";
        if (mDragonModel.empty())
            std::cerr << "Failed to load DragonAttenuation.gltf\n";
        if (mShipModel.empty())
            std::cerr << "Failed to load ally_ship.glb\n";
        else
            std::cout << "ally_ship submeshes: " << mShipModel.size() << '\n';

        // Dim key — AO stays visible on IBL.
        {
            auto key =
                fra::MakeDirectionalLight(glm::vec3(-0.35f, -1.0f, -0.25f),
                                          glm::vec3(1.0f, 0.97f, 0.92f), 0.15f);
            key.castShadows = false;
            mLightService->AddLight(key);
        }

        buildScene();

        std::cout
            << "SSAO Debug — DamagedHelmet + Dragon + ally_ship\n"
            << "RMB look | WASD move | Space/Q up | Ctrl/E down\n"
            << "ImGui: Freya Debug panel (SSAO quality / deferred views / "
               "params / GPU Cull > Show cull AABBs)\n";
    }

    void Update() override
    {
        mOverlay.MarkUpdateStart();
        mOverlay.BeginFrame();

        const float dt = mWindow->GetDeltaTime();
        mCam.Update(dt);

        mRenderer->BeginFrame();

        fra::Camera camera {};
        camera.position = mCam.cameraPos;
        camera.target   = mCam.cameraPos + mCam.Forward();
        camera.up       = glm::vec3(0.0f, 1.0f, 0.0f);
        camera.useFov   = false;
        camera.Apply(*mRenderer);

        mScene.Upload(*mRenderer);

        if (mOverlay.ShowCullAabbs())
            drawCullAabbs();

        const float cpuFrameMs  = dt * 1000.f;
        const float cpuUpdateMs = mOverlay.ElapsedUpdateMs();
        mOverlay.Draw(*mRenderer, *mFreyaOptions, cpuFrameMs, cpuUpdateMs);
        mOverlay.EndFrame(*mRenderer);
    }

  private:
    void buildScene()
    {
        mScene.Clear();
        std::uint32_t nextEntity = 1;

        const auto addPart =
            [&](fra::MeshHandle mesh, fra::MaterialHandle material,
                const glm::mat4& model, fra::Mobility mobility) {
                fra::Scene::Instance inst {};
                inst.mesh        = mesh;
                inst.material    = material;
                inst.transform   = fra::SceneTransform::FromMatrix(model);
                inst.entityId    = nextEntity++;
                inst.castShadows = false;
                inst.mobility    = mobility;
                mScene.Add(inst);
            };

        // Ground slightly below models so baked glTF pivots can rest on it.
        addPart(mGroundMesh, mGroundMaterial,
                glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.05f, 0.0f)),
                fra::Mobility::Static);

        const auto helmetModel = glm::scale(
            glm::translate(glm::mat4(1.0f), glm::vec3(-1.6f, 0.0f, 0.0f)),
            glm::vec3(1.15f));
        for (const auto& part : mHelmetModel)
            addPart(part.mesh, part.material, helmetModel,
                    fra::Mobility::Static);

        const auto dragonModel = glm::scale(
            glm::translate(glm::mat4(1.0f), glm::vec3(1.8f, 0.0f, 0.0f)),
            glm::vec3(1.0f));
        for (const auto& part : mDragonModel)
            addPart(part.mesh, part.material, dragonModel,
                    fra::Mobility::Static);

        const auto shipModel = glm::scale(
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, -2.8f)),
            glm::vec3(100.0f));
        for (const auto& part : mShipModel)
            addPart(part.mesh, part.material, shipModel, fra::Mobility::Static);
    }

    void drawCullAabbs()
    {
        auto& dd = mRenderer->GetDebugDraw();
        mScene.ForEach(
            [&](fra::Scene::InstanceId, const fra::Scene::Instance& inst) {
                FreyaExamples::DrawCullAabb(
                    dd, *mMeshPool, inst.mesh, inst.transform.ToMatrix(),
                    glm::vec4(0.2f, 0.9f, 1.0f, 0.6f));
            });
    }

    fra::Ref<fra::MeshPool>     mMeshPool;
    fra::Ref<fra::MaterialPool> mMaterialPool;
    fra::Ref<fra::LightService> mLightService;
    fra::Ref<fra::FreyaOptions> mFreyaOptions;

    fra::MeshHandle                mGroundMesh {};
    fra::MaterialHandle            mGroundMaterial {};
    std::vector<fra::ModelSubmesh> mHelmetModel;
    std::vector<fra::ModelSubmesh> mDragonModel;
    std::vector<fra::ModelSubmesh> mShipModel;
    fra::Scene                     mScene;

    FreyaExamples::FlyCam       mCam;
    FreyaExamples::DebugOverlay mOverlay;
};

int main(int, const char**)
{
    return fra::RunApp<MainApp>(
        [](fra::FreyaOptionsBuilder& o) {
            o.SetTitle("SSAO Debug")
                .SetWidth(1600)
                .SetHeight(900)
                .SetFullscreen(false)
                .SetVSync(true)
                .WithReverseZ()
                .SetSampleCount(1)
                .SetIblIntensity(0.85f)
                .SetExposure(0.8f)
                .SetShadowQuality(fra::ShadowQuality::Off)
                .SetEnableTaa(false)
                .SetEnableBloom(false)
                .SetSsaoQuality(fra::SsaoQuality::High)
                .SetDeferredDebugView(fra::DeferredDebugView::SsaoBlurred);
        },
        [](skr::LoggingExtension& l) {
            FreyaExamples::ConfigureLogging(l, "SsaoDebug.log");
        });
}
