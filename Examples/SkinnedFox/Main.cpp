#include <Freya/Freya.hpp>
#include <Freya/Vulkan.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

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
                    setLookHeld(false);
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
                mYaw += event.deltaX * 0.12f;
                mPitch -= event.deltaY * 0.12f;
                mPitch = std::clamp(mPitch, -89.0f, 89.0f);
            });

        mRenderer->ClearProjections();

        mSkinned =
            mMeshPool->CreateSkinnedModelFromFile("./Resources/Models/Fox.glb");
        if (mSkinned.meshIds.empty() || mSkinned.skeleton.JointCount() == 0)
        {
            std::cerr << "Failed to load Fox.glb as a skinned model\n";
            return;
        }

        mClipIndex = 0;
        for (std::uint32_t i = 0; i < mSkinned.clips.size(); ++i)
        {
            if (mSkinned.clips[i].name.find("Walk") != std::string::npos)
            {
                mClipIndex = i;
                break;
            }
        }
        if (mSkinned.clips.empty())
            std::cerr << "Fox.glb has no animation clips\n";
        else
            std::cout << "Playing clip '" << mSkinned.clips[mClipIndex].name
                      << "' (" << mSkinned.skeleton.JointCount() << " joints, "
                      << mSkinned.meshIds.size() << " meshes)\n";

        mFoxMaterial = mMaterialPool->Create({
            .albedoFactor    = { 0.82f, 0.45f, 0.18f, 1.f },
            .roughnessFactor = 0.65f,
            .metalnessFactor = 0.0f,
        });

        mGroundMesh = createGroundMesh();
        mGroundMaterial = mMaterialPool->Create({
            .albedoFactor    = { 0.35f, 0.38f, 0.32f, 1.f },
            .roughnessFactor = 0.9f,
        });

        auto key = fra::MakeDirectionalLight(glm::vec3(-0.35f, -1.0f, -0.25f),
                                             glm::vec3(1.0f, 0.97f, 0.92f),
                                             1.2f);
        key.castShadows = true;
        mLightService->AddLight(key);

        mModel =
            glm::scale(glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.02f, 0.f)),
                       glm::vec3(0.02f));
    }

    void Update() override
    {
        const float dt = mWindow->GetDeltaTime();
        mAnimTime += dt;

        if (mLookHeld)
        {
            const float yawRad   = glm::radians(mYaw);
            const float pitchRad = glm::radians(mPitch);
            glm::vec3   front;
            front.x = std::cos(yawRad) * std::cos(pitchRad);
            front.y = std::sin(pitchRad);
            front.z = std::sin(yawRad) * std::cos(pitchRad);
            front   = glm::normalize(front);
            const glm::vec3 right =
                glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));
            const float speed = 8.f * dt;
            if (mKeysHeld.contains(static_cast<std::uint32_t>(fra::KeyCode::W)))
                mCameraPos += front * speed;
            if (mKeysHeld.contains(static_cast<std::uint32_t>(fra::KeyCode::S)))
                mCameraPos -= front * speed;
            if (mKeysHeld.contains(static_cast<std::uint32_t>(fra::KeyCode::A)))
                mCameraPos -= right * speed;
            if (mKeysHeld.contains(static_cast<std::uint32_t>(fra::KeyCode::D)))
                mCameraPos += right * speed;
        }

        mRenderer->BeginFrame();

        if (!mSkinned.clips.empty())
        {
            const auto& clip = mSkinned.clips[mClipIndex];
            auto        pose =
                fra::EvaluateSkeletonPose(mSkinned.skeleton, clip, mAnimTime);
            mRenderer->UploadBoneMatrices(pose);
        }
        else
        {
            const auto rest = fra::EvaluateSkeletonPose(
                mSkinned.skeleton, fra::AnimationClip {}, 0.f);
            mRenderer->UploadBoneMatrices(rest);
        }

        const float yawRad   = glm::radians(mYaw);
        const float pitchRad = glm::radians(mPitch);
        glm::vec3   front;
        front.x = std::cos(yawRad) * std::cos(pitchRad);
        front.y = std::sin(pitchRad);
        front.z = std::sin(yawRad) * std::cos(pitchRad);
        front   = glm::normalize(front);
        mRenderer->UpdateCamera(mCameraPos, mCameraPos + front,
                                glm::vec3(0.f, 1.f, 0.f));

        std::vector<fra::SceneInstanceUpload> instances;
        instances.reserve(mSkinned.meshIds.size() + 1);
        for (std::uint32_t i = 0; i < mSkinned.meshIds.size(); ++i)
        {
            instances.push_back(fra::SceneInstanceUpload {
                .model       = mModel,
                .meshId      = mSkinned.meshIds[i],
                .materialId  = mFoxMaterial,
                .entityId    = 1u + i,
                .castShadows = true,
                .boneOffset  = 0u,
                .boneCount   = mSkinned.skeleton.JointCount(),
            });
        }
        instances.push_back(fra::SceneInstanceUpload {
            .model = glm::scale(
                glm::translate(glm::mat4(1.f), glm::vec3(0.f, -0.05f, 0.f)),
                glm::vec3(40.f, 1.f, 40.f)),
            .meshId      = mGroundMesh,
            .materialId  = mGroundMaterial,
            .entityId    = 100u,
            .castShadows = false,
        });
        mRenderer->UploadSceneInstances(instances);
        mRenderer->EndFrame();
    }

  private:
    void setLookHeld(const bool held)
    {
        mLookHeld = held;
        mWindow->SetMouseGrab(held);
    }

    std::uint32_t createGroundMesh()
    {
        const std::vector<fra::Vertex> vertices = {
            { { -0.5f, 0.f, -0.5f },
              { 1, 1, 1 },
              { 0, 1, 0 },
              { 1, 0, 0 },
              { 0, 0 } },
            { { 0.5f, 0.f, -0.5f },
              { 1, 1, 1 },
              { 0, 1, 0 },
              { 1, 0, 0 },
              { 1, 0 } },
            { { 0.5f, 0.f, 0.5f },
              { 1, 1, 1 },
              { 0, 1, 0 },
              { 1, 0, 0 },
              { 1, 1 } },
            { { -0.5f, 0.f, 0.5f },
              { 1, 1, 1 },
              { 0, 1, 0 },
              { 1, 0, 0 },
              { 0, 1 } },
        };
        const std::vector<std::uint32_t> indices = { 0, 1, 2, 0, 2, 3 };
        return mMeshPool->CreateMesh(vertices, indices);
    }

    skr::Arc<fra::MeshPool>     mMeshPool;
    skr::Arc<fra::TexturePool>  mTexturePool;
    skr::Arc<fra::MaterialPool> mMaterialPool;
    skr::Arc<fra::LightService> mLightService;
    skr::Arc<fra::FreyaOptions> mFreyaOptions;

    fra::SkinnedModel mSkinned;
    std::uint32_t     mClipIndex    = 0;
    std::uint32_t     mFoxMaterial  = 0;
    std::uint32_t     mGroundMesh     = 0;
    std::uint32_t     mGroundMaterial = 0;
    glm::mat4         mModel { 1.f };
    float             mAnimTime = 0.f;

    std::unordered_set<std::uint32_t> mKeysHeld;
    bool                              mLookHeld  = false;
    glm::vec3                         mCameraPos = { 0.8f, 1.2f, 3.5f };
    float                             mYaw       = -90.f;
    float                             mPitch     = -12.f;
};

int main(int, const char**)
{
    const auto app =
        skr::ApplicationBuilder()
            .WithExtension<fra::FreyaExtension>([](fra::FreyaExtension freya) {
                freya.WithOptions([](fra::FreyaOptionsBuilder& freyaOptions) {
                    freyaOptions.SetTitle("SkinnedFox — Walk [RMB+WASD]")
                        .SetWidth(1600)
                        .SetHeight(900)
                        .SetVSync(false)
                        .WithReverseZ()
                        .SetIblIntensity(0.2f)
                        .SetShadowQuality(fra::ShadowQuality::Medium)
                        .SetFullscreen(false);
                });
            })
            .Build<MainApp>();

    app->Run();
    return 0;
}
