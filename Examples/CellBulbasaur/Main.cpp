#include <Freya/Vulkan.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <unordered_set>
#include <vector>

class MainApp final : public fra::AbstractApplication
{
  public:
    explicit MainApp(const skr::Arc<skr::ServiceProvider>& serviceProvider) :
        AbstractApplication(serviceProvider)
    {
        mMeshPool     = serviceProvider->GetService<fra::MeshPool>();
        mTexturePool  = serviceProvider->GetService<fra::TexturePool>();
        mMaterialPool = serviceProvider->GetService<fra::MaterialPool>();
        mLightService = serviceProvider->GetService<fra::LightService>();
        mFreyaOptions = serviceProvider->GetService<fra::FreyaOptions>();
        mServices     = serviceProvider;
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
                if (event.key == fra::KeyCode::F4)
                {
                    if (mCellEffect)
                    {
                        mCellEffect->SetEnabled(!mCellEffect->Enabled());
                        std::cout
                            << "Cell shader: "
                            << (mCellEffect->Enabled() ? "on" : "off") << '\n';
                        updateTitle();
                    }
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

        mCellEffect =
            mServices->GetService<fra::FullscreenEffectBuilder>()
                ->SetName("Cell")
                .SetFragment("Cell/cell.frag.spv")
                .SetInputs(
                    { fra::EffectInput::SceneColor, fra::EffectInput::Depth,
                      fra::EffectInput::Normal })
                .SetPushConstantSize(sizeof(fra::CellPushConstants))
                .Build();
        if (mCellEffect)
        {
            fra::CellPushConstants cell {};
            cell.bands           = 5.0f;
            cell.edgeDepthScale  = 120.0f;
            cell.edgeNormalScale = 2.4f;
            cell.strength        = 1.0f;
            cell.edgeColor       = { 0.05f, 0.08f, 0.04f, 1.0f };
            cell.reverseZ        = mFreyaOptions->ReverseZ ? 1.0f : 0.0f;
            mCellEffect->SetPushConstants(cell);
            mRenderer->InsertFrameStage("Bloom", mCellEffect->MakeStage());
        }

        mGroundMesh     = createGroundMesh();
        mGroundMaterial = mMaterialPool->Create({
            .albedoFactor    = { 0.55f, 0.62f, 0.38f, 1.0f },
            .roughnessFactor = 0.95f,
            .metalnessFactor = 0.0f,
        });

        const auto eyeAlbedo =
            mTexturePool->CreateTextureFromFile("./Resources/Textures/eye.png");
        const auto bodyBAlbedo = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/bodyB.png");
        const auto bodyAAlbedo = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/bodyA.png");

        mEyeMaterial   = mMaterialPool->Create({
            .albedo          = eyeAlbedo,
            .roughnessFactor = 0.35f,
            .metalnessFactor = 0.0f,
        });
        mBodyBMaterial = mMaterialPool->Create({
            .albedo          = bodyBAlbedo,
            .roughnessFactor = 0.7f,
            .metalnessFactor = 0.0f,
        });
        mBodyAMaterial = mMaterialPool->Create({
            .albedo          = bodyAAlbedo,
            .roughnessFactor = 0.7f,
            .metalnessFactor = 0.0f,
        });

        mSkinned = mMeshPool->CreateSkinnedModelFromFile(
            "./Resources/Models/bulbasaur.glb");
        if (mSkinned.meshIds.empty() || mSkinned.skeleton.JointCount() == 0)
        {
            std::cerr << "Failed to load bulbasaur.glb as a skinned model\n";
        }
        else
        {
            std::cout << "bulbasaur meshes: " << mSkinned.meshIds.size()
                      << " joints: " << mSkinned.skeleton.JointCount()
                      << " clips: " << mSkinned.clips.size() << '\n';
            for (const auto& clip : mSkinned.clips)
                std::cout << "  clip: " << clip.name << " (" << clip.duration
                          << "s)\n";
            mIdleClip = findClip(mSkinned, "idle");
            if (!mIdleClip && !mSkinned.clips.empty())
                mIdleClip = &mSkinned.clips.front();
        }

        {
            auto key =
                fra::MakeDirectionalLight(glm::vec3(-0.4f, -1.0f, -0.35f),
                                          glm::vec3(1.0f, 0.98f, 0.92f), 1.6f);
            key.castShadows = true;
            mLightService->AddLight(key);
        }
        mLightService->AddLight(fra::MakePointLight(
            glm::vec3(2.2f, 2.4f, 2.0f), glm::vec3(0.85f, 0.95f, 0.7f), 6.0f,
            8.0f));

        buildSceneInstances();
        updateTitle();

        std::cout << "CellBulbasaur — F4 toggles cell+edges\n"
                     "RMB look | WASD move | Space/Q up | Ctrl/E down\n";
    }

    void Update() override
    {
        const float dt = mWindow->GetDeltaTime();
        updateCamera(dt);

        mRenderer->BeginFrame();

        const glm::vec3 forward = cameraForward();
        mRenderer->UpdateCamera(
            mCameraPos, mCameraPos + forward, glm::vec3(0.0f, 1.0f, 0.0f));

        const auto jointCount = mSkinned.skeleton.JointCount();
        if (jointCount > 0)
        {
            fra::LocalPose local;
            if (mIdleClip)
            {
                mAnimTime += dt;
                local = fra::SampleClip(
                    mSkinned.skeleton, *mIdleClip, mAnimTime, true);
            }
            else
            {
                local = fra::RestLocalPose(mSkinned.skeleton);
            }
            mRenderer->UploadBoneMatrices(
                fra::PoseToSkinMatrices(mSkinned.skeleton, local));
        }

        std::vector<fra::SceneInstanceUpload> instances;
        instances.reserve(mInstances.size());
        for (const auto& inst : mInstances)
        {
            instances.push_back({
                .model       = inst.model,
                .meshId      = inst.meshId,
                .materialId  = inst.materialId,
                .entityId    = inst.entityId,
                .castShadows = inst.castShadows,
                .boneOffset  = inst.boneOffset,
                .boneCount   = inst.boneCount,
            });
        }
        mRenderer->UploadSceneInstances(instances);
        mRenderer->EndFrame();
    }

  private:
    static constexpr float kMoveSpeed        = 6.0f;
    static constexpr float kMouseSensitivity = 0.12f;
    static constexpr float kModelScale       = 100.0f;

    struct Instance
    {
        glm::mat4     model {};
        std::uint32_t meshId      = 0;
        std::uint32_t materialId  = 0;
        std::uint32_t entityId    = 0;
        bool          castShadows = true;
        std::uint32_t boneOffset  = fra::kNoSkin;
        std::uint32_t boneCount   = 0;
    };

    static const fra::AnimationClip* findClip(const fra::SkinnedModel& model,
                                              std::string_view         needle)
    {
        for (const auto& clip : model.clips)
        {
            if (clip.name.find(needle) != std::string::npos)
                return &clip;
        }
        return nullptr;
    }

    std::uint32_t materialForMesh(std::size_t index) const
    {
        switch (index % 3)
        {
            case 0:
                return mEyeMaterial;
            case 1:
                return mBodyBMaterial;
            default:
                return mBodyAMaterial;
        }
    }

    std::uint32_t createGroundMesh()
    {
        constexpr float half = 8.0f;
        const auto      tint = glm::vec3(0.72f, 0.78f, 0.55f);
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

        {
            Instance ground {};
            ground.model =
                glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.02f, 0.0f));
            ground.meshId      = mGroundMesh;
            ground.materialId  = mGroundMaterial;
            ground.entityId    = nextEntity++;
            ground.castShadows = false;
            mInstances.push_back(ground);
        }

        auto model = glm::rotate(
            glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        model             = glm::scale(model, glm::vec3(kModelScale));
        const auto joints = mSkinned.skeleton.JointCount();
        for (std::size_t i = 0; i < mSkinned.meshIds.size(); ++i)
        {
            Instance inst {};
            inst.model      = model;
            inst.meshId     = mSkinned.meshIds[i];
            inst.materialId = materialForMesh(i);
            inst.entityId   = nextEntity++;
            inst.boneOffset = joints > 0 ? 0u : fra::kNoSkin;
            inst.boneCount  = joints;
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

    void updateTitle()
    {
        mFreyaOptions->title =
            std::string("CellBulbasaur | cell ") +
            (mCellEffect && mCellEffect->Enabled() ? "on" : "off") + " [F4]";
    }

    skr::Arc<skr::ServiceProvider>  mServices;
    skr::Arc<fra::FullscreenEffect> mCellEffect;
    skr::Arc<fra::MeshPool>         mMeshPool;
    skr::Arc<fra::TexturePool>      mTexturePool;
    skr::Arc<fra::MaterialPool>     mMaterialPool;
    skr::Arc<fra::LightService>     mLightService;
    skr::Arc<fra::FreyaOptions>     mFreyaOptions;

    std::uint32_t             mGroundMesh     = 0;
    std::uint32_t             mGroundMaterial = 0;
    std::uint32_t             mEyeMaterial    = 0;
    std::uint32_t             mBodyAMaterial  = 0;
    std::uint32_t             mBodyBMaterial  = 0;
    fra::SkinnedModel         mSkinned;
    const fra::AnimationClip* mIdleClip = nullptr;
    float                     mAnimTime = 0.0f;
    std::vector<Instance>     mInstances;

    glm::vec3 mCameraPos { 0.0f, 1.6f, 4.2f };
    float     mYaw      = -90.0f;
    float     mPitch    = -8.0f;
    bool      mLookHeld = false;

    std::unordered_set<std::uint32_t> mKeysHeld;
};

int main(int, const char**)
{
    const auto app =
        skr::ApplicationBuilder()
            .WithExtension<fra::FreyaExtension>([](fra::FreyaExtension freya) {
                freya.WithOptions([](fra::FreyaOptionsBuilder& o) {
                    o.SetTitle("CellBulbasaur")
                        .SetWidth(1600)
                        .SetHeight(900)
                        .SetFullscreen(false)
                        .SetVSync(true)
                        .WithReverseZ()
                        .SetSampleCount(1)
                        .SetIblIntensity(0.25f)
                        .SetExposure(0.85f)
                        .SetShadowQuality(fra::ShadowQuality::High)
                        .SetTaaQuality(fra::TaaQuality::Off)
                        .SetBloomQuality(fra::BloomQuality::Off)
                        .SetSsaoQuality(fra::SsaoQuality::Medium);
                });
            })
            .Build<MainApp>();

    app->Run();
    return 0;
}
