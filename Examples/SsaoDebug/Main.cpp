#include <Freya/Freya.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

namespace
{
    const char* SsaoQualityName(fra::SsaoQuality q)
    {
        switch (q)
        {
            case fra::SsaoQuality::Low:
                return "Low";
            case fra::SsaoQuality::Medium:
                return "Medium";
            case fra::SsaoQuality::High:
                return "High";
            case fra::SsaoQuality::Ultra:
                return "Ultra";
            case fra::SsaoQuality::Off:
                return "Off";
        }
        return "?";
    }

    const char* SsaoDebugViewName(fra::SsaoDebugView v)
    {
        switch (v)
        {
            case fra::SsaoDebugView::None:
                return "Lit";
            case fra::SsaoDebugView::Blurred:
                return "AO Blurred";
            case fra::SsaoDebugView::Raw:
                return "AO Raw";
        }
        return "?";
    }
} // namespace

class MainApp final : public fra::AbstractApplication
{
  public:
    explicit MainApp(const skr::Arc<skr::ServiceProvider>& serviceProvider) :
        AbstractApplication(serviceProvider)
    {
        mMeshPool     = serviceProvider->GetService<fra::MeshPool>();
        mMaterialPool = serviceProvider->GetService<fra::MaterialPool>();
        mLightService = serviceProvider->GetService<fra::LightService>();
        mFreyaOptions = serviceProvider->GetService<fra::FreyaOptions>();
    }

    void StartUp() override
    {
        mEventManager->Subscribe<fra::KeyPressedEvent>(
            [this](const fra::KeyPressedEvent& event) {
                mKeysHeld.insert(static_cast<std::uint32_t>(event.key));
            });

        mEventManager->Subscribe<fra::KeyReleasedEvent>(
            [this](const fra::KeyReleasedEvent& event) {
                mKeysHeld.erase(static_cast<std::uint32_t>(event.key));

                if (event.key == fra::KeyCode::Escape && mLookHeld)
                {
                    setLookHeld(false);
                    return;
                }
                if (event.key == fra::KeyCode::F6)
                {
                    cycleSsaoQuality();
                    return;
                }
                if (event.key == fra::KeyCode::V)
                {
                    cycleDebugView();
                    return;
                }
                if (event.key == fra::KeyCode::R)
                {
                    resetSsaoParams();
                    return;
                }

                const bool  increase = isHeld(fra::KeyCode::LShift) ||
                                       isHeld(fra::KeyCode::RShift);
                const float sign     = increase ? 1.0f : -1.0f;

                if (event.key == fra::KeyCode::Num1 ||
                    event.key == fra::KeyCode::Kp1)
                {
                    nudgeRadius(sign * 0.05f);
                    return;
                }
                if (event.key == fra::KeyCode::Num2 ||
                    event.key == fra::KeyCode::Kp2)
                {
                    nudgeBias(sign * 0.005f);
                    return;
                }
                if (event.key == fra::KeyCode::Num3 ||
                    event.key == fra::KeyCode::Kp3)
                {
                    nudgePower(sign * 0.05f);
                    return;
                }
                if (event.key == fra::KeyCode::Num4 ||
                    event.key == fra::KeyCode::Kp4)
                {
                    nudgeIntensity(sign * 0.05f);
                    return;
                }
            });

        mEventManager->Subscribe<fra::MouseButtonPressedEvent>(
            [this](const fra::MouseButtonPressedEvent& event) {
                if (event.button == fra::MouseButton::Right)
                    setLookHeld(true);
            });

        mEventManager->Subscribe<fra::MouseButtonReleasedEvent>(
            [this](const fra::MouseButtonReleasedEvent& event) {
                if (event.button == fra::MouseButton::Right)
                    setLookHeld(false);
            });

        mEventManager->Subscribe<fra::MouseMoveEvent>(
            [this](const fra::MouseMoveEvent& event) {
                if (!mLookHeld)
                    return;
                mYaw += event.deltaX * kMouseSensitivity;
                mPitch -= event.deltaY * kMouseSensitivity;
                mPitch = std::clamp(mPitch, -89.0f, 89.0f);
            });

        mRenderer->ClearProjections();

        mGroundMesh     = createGroundMesh();
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
            << "V  cycle view: Lit / AO Blurred / AO Raw\n"
            << "F6 cycle SSAO quality (Low→Med→High→Ultra→Off)\n"
            << "1–4 nudge radius/bias/power/intensity (−); Shift+1–4 (+)\n"
            << "R  reset params to current quality preset\n";

        updateTitle();
    }

    void Update() override
    {
        const float dt = mWindow->GetDeltaTime();
        updateCamera(dt);

        mRenderer->BeginFrame();

        const glm::vec3 forward = cameraForward();
        mRenderer->UpdateCamera(
            mCameraPos, mCameraPos + forward, glm::vec3(0.0f, 1.0f, 0.0f));

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
        mRenderer->EndFrame();
    }

  private:
    static constexpr float kMoveSpeed        = 10.0f;
    static constexpr float kMouseSensitivity = 0.12f;

    struct Instance
    {
        glm::mat4     model {};
        std::uint32_t meshId     = 0;
        std::uint32_t materialId = 0;
        std::uint32_t entityId   = 0;
    };

    std::uint32_t createGroundMesh()
    {
        constexpr float half = 20.0f;
        const auto      tint = glm::vec3(0.72f, 0.72f, 0.76f);
        const auto      up   = glm::vec3(0.0f, 1.0f, 0.0f);
        const auto      tan  = glm::vec3(1.0f, 0.0f, 0.0f);

        const std::vector<fra::Vertex> vertices = {
            { { -half, 0.0f, -half }, tint, up, tan, { 0.0f, 0.0f } },
            { { half, 0.0f, -half }, tint, up, tan, { 1.0f, 0.0f } },
            { { half, 0.0f, half }, tint, up, tan, { 1.0f, 1.0f } },
            { { -half, 0.0f, half }, tint, up, tan, { 0.0f, 1.0f } },
        };
        const std::vector<std::uint32_t> indices = { 0, 3, 2, 0, 2, 1 };
        return mMeshPool->CreateMesh(vertices, indices);
    }

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

    [[nodiscard]] bool isHeld(fra::KeyCode key) const
    {
        return mKeysHeld.contains(static_cast<std::uint32_t>(key));
    }

    void setLookHeld(bool held)
    {
        mLookHeld = held;
        mWindow->SetMouseGrab(held);
    }

    [[nodiscard]] glm::vec3 cameraForward() const
    {
        const float yawRad   = glm::radians(mYaw);
        const float pitchRad = glm::radians(mPitch);
        return glm::normalize(glm::vec3 {
            std::cos(pitchRad) * std::cos(yawRad),
            std::sin(pitchRad),
            std::cos(pitchRad) * std::sin(yawRad),
        });
    }

    void updateCamera(float dt)
    {
        const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
        const glm::vec3 look = cameraForward();
        const glm::vec3 forward =
            glm::normalize(glm::vec3(look.x, 0.0f, look.z));
        const glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
        glm::vec3       move(0.0f);

        if (isHeld(fra::KeyCode::W))
            move += forward;
        if (isHeld(fra::KeyCode::S))
            move -= forward;
        if (isHeld(fra::KeyCode::D))
            move += right;
        if (isHeld(fra::KeyCode::A))
            move -= right;
        if (isHeld(fra::KeyCode::Space) || isHeld(fra::KeyCode::Q))
            move += worldUp;
        if (isHeld(fra::KeyCode::LCtrl) || isHeld(fra::KeyCode::RCtrl) ||
            isHeld(fra::KeyCode::E))
            move -= worldUp;

        if (glm::length(move) > 1e-4f)
            mCameraPos += glm::normalize(move) * kMoveSpeed * dt;
    }

    void cycleSsaoQuality()
    {
        const auto       current = mRenderer->GetSsaoQuality();
        fra::SsaoQuality next    = fra::SsaoQuality::Low;
        switch (current)
        {
            case fra::SsaoQuality::Low:
                next = fra::SsaoQuality::Medium;
                break;
            case fra::SsaoQuality::Medium:
                next = fra::SsaoQuality::High;
                break;
            case fra::SsaoQuality::High:
                next = fra::SsaoQuality::Ultra;
                break;
            case fra::SsaoQuality::Ultra:
                next = fra::SsaoQuality::Off;
                break;
            case fra::SsaoQuality::Off:
                next = fra::SsaoQuality::Low;
                break;
        }
        mRenderer->SetSsaoQuality(next);
        std::cout << "SSAO quality: " << SsaoQualityName(next) << '\n';
        updateTitle();
    }

    void cycleDebugView()
    {
        const auto         current = mRenderer->GetSsaoDebugView();
        fra::SsaoDebugView next    = fra::SsaoDebugView::None;
        switch (current)
        {
            case fra::SsaoDebugView::None:
                next = fra::SsaoDebugView::Blurred;
                break;
            case fra::SsaoDebugView::Blurred:
                next = fra::SsaoDebugView::Raw;
                break;
            case fra::SsaoDebugView::Raw:
                next = fra::SsaoDebugView::None;
                break;
        }
        mRenderer->SetSsaoDebugView(next);
        std::cout << "SSAO view: " << SsaoDebugViewName(next) << '\n';
        updateTitle();
    }

    void resetSsaoParams()
    {
        const auto quality = mRenderer->GetSsaoQuality();
        if (quality == fra::SsaoQuality::Off)
        {
            std::cout << "SSAO is Off — enable a quality preset first (F6)\n";
            return;
        }

        switch (quality)
        {
            case fra::SsaoQuality::Low:
                mRenderer->SetSsaoRadius(0.4f);
                mRenderer->SetSsaoBias(0.03f);
                mRenderer->SetSsaoPower(1.4f);
                mRenderer->SetSsaoIntensity(0.5f);
                break;
            case fra::SsaoQuality::Medium:
                mRenderer->SetSsaoRadius(0.5f);
                mRenderer->SetSsaoBias(0.025f);
                mRenderer->SetSsaoPower(1.5f);
                mRenderer->SetSsaoIntensity(0.5f);
                break;
            case fra::SsaoQuality::High:
                mRenderer->SetSsaoRadius(0.65f);
                mRenderer->SetSsaoBias(0.025f);
                mRenderer->SetSsaoPower(1.6f);
                mRenderer->SetSsaoIntensity(0.5f);
                break;
            case fra::SsaoQuality::Ultra:
                mRenderer->SetSsaoRadius(0.8f);
                mRenderer->SetSsaoBias(0.02f);
                mRenderer->SetSsaoPower(1.7f);
                mRenderer->SetSsaoIntensity(0.5f);
                break;
            case fra::SsaoQuality::Off:
                break;
        }
        std::cout << "SSAO params reset to " << SsaoQualityName(quality)
                  << " preset\n";
        updateTitle();
    }

    void nudgeRadius(float delta)
    {
        mRenderer->SetSsaoRadius(mRenderer->GetSsaoRadius() + delta);
        updateTitle();
    }

    void nudgeBias(float delta)
    {
        mRenderer->SetSsaoBias(mRenderer->GetSsaoBias() + delta);
        updateTitle();
    }

    void nudgePower(float delta)
    {
        mRenderer->SetSsaoPower(mRenderer->GetSsaoPower() + delta);
        updateTitle();
    }

    void nudgeIntensity(float delta)
    {
        mRenderer->SetSsaoIntensity(mRenderer->GetSsaoIntensity() + delta);
        updateTitle();
    }

    void updateTitle()
    {
        char buf[256];
        std::snprintf(
            buf, sizeof(buf),
            "SSAO Debug [%s | %s]  r=%.2f b=%.3f p=%.2f i=%.2f  [V view F6 q]",
            SsaoDebugViewName(mRenderer->GetSsaoDebugView()),
            SsaoQualityName(mRenderer->GetSsaoQuality()),
            mRenderer->GetSsaoRadius(), mRenderer->GetSsaoBias(),
            mRenderer->GetSsaoPower(), mRenderer->GetSsaoIntensity());
        mFreyaOptions->title = buf;
    }

    skr::Arc<fra::MeshPool>     mMeshPool;
    skr::Arc<fra::MaterialPool> mMaterialPool;
    skr::Arc<fra::LightService> mLightService;
    skr::Arc<fra::FreyaOptions> mFreyaOptions;

    std::uint32_t              mGroundMesh     = 0;
    std::uint32_t              mGroundMaterial = 0;
    std::vector<fra::ModelSubmesh> mHelmetModel;
    std::vector<fra::ModelSubmesh> mDragonModel;
    std::vector<fra::ModelSubmesh> mShipModel;
    std::vector<Instance>      mInstances;

    glm::vec3 mCameraPos { 0.2f, 1.4f, 4.8f };
    float     mYaw      = -95.0f;
    float     mPitch    = -12.0f;
    bool      mLookHeld = false;

    std::unordered_set<std::uint32_t> mKeysHeld;
};

int main(int, const char**)
{
    const auto app =
        skr::ApplicationBuilder()
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
                        .SetSsaoDebugView(fra::SsaoDebugView::Blurred);
                });
            })
            .Build<MainApp>();

    app->Run();
    return 0;
}
