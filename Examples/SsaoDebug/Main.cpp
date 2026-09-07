#include <Freya/Freya.hpp>

#include <FreyaExamples/DebugOverlay.hpp>
#include <FreyaExamples/ExampleLogging.hpp>
#include <FreyaExamples/FlyCam.hpp>
#include <FreyaExamples/GroundMesh.hpp>

#include <iostream>
#include <vector>

class MainApp final : public fra::AbstractApplication
{
  public:
    explicit MainApp(const skr::Arc<skr::ServiceProvider>& serviceProvider) :
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

        buildSceneInstances();

        std::cout
            << "SSAO Debug — DamagedHelmet + Dragon + ally_ship\n"
            << "RMB look | WASD move | Space/Q up | Ctrl/E down\n"
            << "ImGui: Freya Debug panel (SSAO quality / deferred views / "
               "params)\n";
    }

    void Update() override
    {
        mOverlay.MarkUpdateStart();
        mOverlay.BeginFrame();

        const float dt = mWindow->GetDeltaTime();
        mCam.Update(dt);

        mRenderer->BeginFrame();
        mCam.Apply(*mRenderer);

        std::vector<fra::SceneInstanceUpload> instances;
        instances.reserve(mInstances.size());
        for (const auto& inst : mInstances)
        {
            instances.push_back({
                .model       = inst.model,
                .meshId      = inst.meshId,
                .materialId  = inst.materialId,
                .entityId    = inst.entityId,
                .castShadows = false,
            });
        }
        mRenderer->UploadSceneInstances(instances);

        const float cpuFrameMs  = dt * 1000.f;
        const float cpuUpdateMs = mOverlay.ElapsedUpdateMs();
        mOverlay.Draw(*mRenderer, *mFreyaOptions, cpuFrameMs, cpuUpdateMs);
        mOverlay.EndFrame(*mRenderer);
    }

  private:
    struct Instance
    {
        glm::mat4     model {};
        std::uint32_t meshId     = 0;
        std::uint32_t materialId = 0;
        std::uint32_t entityId   = 0;
    };

    void buildSceneInstances()
    {
        mInstances.clear();
        std::uint32_t nextEntity = 1;

        // Ground slightly below models so baked glTF pivots can rest on it.
        {
            Instance ground {};
            ground.model =
                glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.05f, 0.0f));
            ground.meshId     = mGroundMesh;
            ground.materialId = mGroundMaterial;
            ground.entityId   = nextEntity++;
            mInstances.push_back(ground);
        }

        const auto helmetModel = glm::scale(
            glm::translate(glm::mat4(1.0f), glm::vec3(-1.6f, 0.0f, 0.0f)),
            glm::vec3(1.15f));
        for (const auto& part : mHelmetModel)
        {
            Instance inst {};
            inst.model      = helmetModel;
            inst.meshId     = part.meshId;
            inst.materialId = part.materialId;
            inst.entityId   = nextEntity++;
            mInstances.push_back(inst);
        }

        // Node transforms are baked via PreTransformVertices.
        const auto dragonModel = glm::scale(
            glm::translate(glm::mat4(1.0f), glm::vec3(1.8f, 0.0f, 0.0f)),
            glm::vec3(1.0f));
        for (const auto& part : mDragonModel)
        {
            Instance inst {};
            inst.model      = dragonModel;
            inst.meshId     = part.meshId;
            inst.materialId = part.materialId;
            inst.entityId   = nextEntity++;
            mInstances.push_back(inst);
        }

        // glTF node scales by 0.01; PreTransformVertices bakes that → ~3 cm.
        // Scale ×100 → ~3 m, level with the rest of the SSAO props.
        const auto shipModel = glm::scale(
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, -2.8f)),
            glm::vec3(100.0f));
        for (const auto& part : mShipModel)
        {
            Instance inst {};
            inst.model      = shipModel;
            inst.meshId     = part.meshId;
            inst.materialId = part.materialId;
            inst.entityId   = nextEntity++;
            mInstances.push_back(inst);
        }
    }

    skr::Arc<fra::MeshPool>     mMeshPool;
    skr::Arc<fra::MaterialPool> mMaterialPool;
    skr::Arc<fra::LightService> mLightService;
    skr::Arc<fra::FreyaOptions> mFreyaOptions;

    std::uint32_t                  mGroundMesh     = 0;
    std::uint32_t                  mGroundMaterial = 0;
    std::vector<fra::ModelSubmesh> mHelmetModel;
    std::vector<fra::ModelSubmesh> mDragonModel;
    std::vector<fra::ModelSubmesh> mShipModel;
    std::vector<Instance>          mInstances;

    FreyaExamples::FlyCam       mCam;
    FreyaExamples::DebugOverlay mOverlay;
};

int main(int, const char**)
{
    const auto app =
        skr::ApplicationBuilder()
            .WithExtension<skr::LoggingExtension>([](skr::LoggingExtension& l) {
                FreyaExamples::ConfigureLogging(l, "SsaoDebug.log");
            })
            .WithExtension<fra::FreyaExtension>([](fra::FreyaExtension freya) {
                freya.WithOptions([](fra::FreyaOptionsBuilder& o) {
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
                        .SetDeferredDebugView(
                            fra::DeferredDebugView::SsaoBlurred);
                });
            })
            .Build<MainApp>();

    app->Run();
    return 0;
}
