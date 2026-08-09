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

namespace
{
    constexpr std::uint32_t kFoxGridX    = 40;
    constexpr std::uint32_t kFoxGridZ    = 25;
    constexpr float         kFoxSpacing  = 2.1f;
    constexpr float         kFoxScale    = 0.02f;
    constexpr float         kRunPlayback = 1.85f;

    const fra::AnimationClip* findClip(const fra::SkinnedModel& model,
                                       std::string_view         needle)
    {
        for (const auto& clip : model.clips)
        {
            if (clip.name.find(needle) != std::string::npos)
                return &clip;
        }
        return nullptr;
    }

    std::int32_t findJointAny(const fra::Skeleton& skeleton,
                              std::initializer_list<const char*>
                                  needles)
    {
        for (const char* n : needles)
        {
            const auto i = fra::FindJointIndex(skeleton, n);
            if (i >= 0)
                return i;
        }
        return -1;
    }

    float speedPreset(const std::uint32_t index)
    {
        switch (index % 5u)
        {
            case 0:
                return 0.f;
            case 1:
                return 0.55f;
            case 2:
                return 1.0f;
            case 3:
                return 1.45f;
            default:
                return 2.0f;
        }
    }

    float locoMetersPerSec(const float speed)
    {
        if (speed <= 0.1f)
            return 0.f;
        if (speed <= 1.f)
            return glm::mix(0.f, 1.1f, speed);
        return glm::mix(1.1f, 3.2f, std::clamp((speed - 1.f), 0.f, 1.f));
    }

    fra::BoneMask makeUpperBodyMask(const fra::Skeleton& skeleton)
    {
        fra::BoneMask mask = fra::BoneMask::Filled(skeleton.JointCount(), 0.f);

        const char* roots[] = { "Spine", "spine", "Chest", "chest",
                                "Neck",  "neck",  "Head",  "head" };
        bool        any     = false;
        for (const char* needle : roots)
        {
            const auto idx = fra::FindJointIndex(skeleton, needle);
            if (idx < 0)
                continue;
            mask.SetSubtree(skeleton, static_cast<std::uint32_t>(idx), 1.f);
            any = true;
        }

        if (!any && skeleton.JointCount() > 0)
        {
            const auto mid = skeleton.JointCount() / 2u;
            for (std::uint32_t i = mid; i < skeleton.JointCount(); ++i)
                mask.SetJoint(i, 1.f);
        }
        return mask;
    }
} // namespace

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
                onKeyPressed(event.key);
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

        const auto* idle = findClip(mSkinned, "Survey");
        const auto* walk = findClip(mSkinned, "Walk");
        const auto* run  = findClip(mSkinned, "Run");
        if (!idle)
            idle = mSkinned.clips.empty() ? nullptr : &mSkinned.clips[0];
        if (!walk)
            walk = idle;
        if (!run)
            run = walk;

        if (!(idle && walk && run))
        {
            std::cerr << "Fox.glb has no animation clips\n";
            return;
        }

        mUpperClip = idle;
        mUpperMask = makeUpperBodyMask(mSkinned.skeleton);

        mHeadJoint = findJointAny(mSkinned.skeleton, { "Head", "head" });
        const auto thigh = findJointAny(
            mSkinned.skeleton, { "LeftLeg01", "LeftUpLeg", "LeftUpperLeg" });
        const auto shin = findJointAny(
            mSkinned.skeleton, { "LeftLeg02", "LeftLowerLeg", "LeftShin" });
        const auto foot = findJointAny(
            mSkinned.skeleton, { "LeftFoot01", "LeftFoot", "Foot_L" });

        mIkReady = false;
        if (thigh >= 0 && shin >= 0 && foot >= 0 && thigh != shin &&
            shin != foot)
        {
            mLegChain.root = static_cast<std::uint32_t>(thigh);
            mLegChain.mid  = static_cast<std::uint32_t>(shin);
            mLegChain.tip  = static_cast<std::uint32_t>(foot);
            mIkReady       = true;
        }

        std::cout << "Rig joints: head=" << mHeadJoint
                  << " leg=" << mLegChain.root << "/" << mLegChain.mid << "/"
                  << mLegChain.tip << (mIkReady ? " (IK ok)" : " (IK off)")
                  << '\n';

        const auto     jointCount = mSkinned.skeleton.JointCount();
        constexpr auto foxCount   = kFoxGridX * kFoxGridZ;
        const float    originX =
            -0.5f * static_cast<float>(kFoxGridX - 1) * kFoxSpacing;
        const float originZ =
            -0.5f * static_cast<float>(kFoxGridZ - 1) * kFoxSpacing;

        std::uint32_t upperCount = 0;
        std::uint32_t lookCount  = 0;
        std::uint32_t ikCount    = 0;
        std::uint32_t rootCount  = 0;
        mFoxes.reserve(foxCount);
        for (std::uint32_t z = 0; z < kFoxGridZ; ++z)
        {
            for (std::uint32_t x = 0; x < kFoxGridX; ++x)
            {
                const std::uint32_t i = z * kFoxGridX + x;
                FoxActor            fox;
                fox.speed         = speedPreset(i);
                fox.boneOffset    = i * jointCount;
                fox.useUpperLayer = (i % 4u) == 0u;
                fox.useLookAt     = mHeadJoint >= 0 && (i % 5u) == 1u;
                fox.useIk         = mIkReady && (i % 13u) == 0u;
                fox.useRootMotion = fox.speed >= 1.5f;
                if (fox.useUpperLayer)
                    ++upperCount;
                if (fox.useLookAt)
                    ++lookCount;
                if (fox.useIk)
                    ++ikCount;
                if (fox.useRootMotion)
                    ++rootCount;

                fox.model = glm::scale(
                    glm::translate(
                        glm::mat4(1.f),
                        glm::vec3(
                            originX + static_cast<float>(x) * kFoxSpacing,
                            0.02f,
                            originZ + static_cast<float>(z) * kFoxSpacing)),
                    glm::vec3(kFoxScale));

                fox.graph =
                    fra::AnimGraphBuilder()
                        .SetSkeleton(&mSkinned.skeleton)
                        .ParamFloat("Speed", fox.speed)
                        .Blend1DState("Loco", "Speed")
                        .AddBlendSample(0.f, *idle)
                        .AddBlendSample(1.f, *walk)
                        .AddBlendSample(2.f, *run, true, kRunPlayback)
                        .Entry("Loco")
                        .Build();

                (void) fox.graph.Evaluate(0.17f * static_cast<float>(i % 17));
                fox.upperTime = 0.11f * static_cast<float>(i % 13);
                mFoxes.push_back(std::move(fox));
            }
        }

        std::cout << "Anim stack — " << foxCount
                  << " foxes | upper=" << upperCount << " look=" << lookCount
                  << " ik=" << ikCount << " rootDrive=" << rootCount << '\n'
                  << "  Keys: 1/2/3 presets   Up/Down=Speed\n";

        mFoxMaterial = mMaterialPool->Create({
            .albedoFactor    = { 0.82f, 0.45f, 0.18f, 1.f },
            .roughnessFactor = 0.65f,
            .metalnessFactor = 0.0f,
        });

        mGroundMesh     = createGroundMesh();
        mGroundMaterial = mMaterialPool->Create({
            .albedoFactor    = { 0.35f, 0.38f, 0.32f, 1.f },
            .roughnessFactor = 0.9f,
        });

        auto key = fra::MakeDirectionalLight(glm::vec3(-0.35f, -1.0f, -0.25f),
                                             glm::vec3(1.0f, 0.97f, 0.92f),
                                             1.2f);
        key.castShadows = true;
        mLightService->AddLight(key);

        mCameraPos = { 0.f, 28.f, 55.f };
        mPitch     = -28.f;
    }

    void Update() override
    {
        const float dt = mWindow->GetDeltaTime();
        mAnimClock += dt;

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
            const float speed = 12.f * dt;
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

        const auto             jointCount = mSkinned.skeleton.JointCount();
        std::vector<glm::mat4> allBones;
        allBones.reserve(mFoxes.size() * jointCount);
        for (auto& fox : mFoxes)
        {
            auto local = fox.graph.Evaluate(dt);
            if (fox.useUpperLayer && mUpperClip)
            {
                fox.upperTime += dt;
                if (mUpperClip->duration > 0.f)
                {
                    fox.upperTime =
                        std::fmod(fox.upperTime, mUpperClip->duration);
                    if (fox.upperTime < 0.f)
                        fox.upperTime += mUpperClip->duration;
                }
                const auto upper = fra::SampleClip(
                    mSkinned.skeleton, *mUpperClip, fox.upperTime, true);
                local = fra::BlendMasked(local, upper, mUpperMask, 0.85f);
            }

            // Procedural root drive (Fox clips are in-place).
            if (fox.useRootMotion)
            {
                fox.model = fra::DrivePlanarLocomotion(
                    fox.model, locoMetersPerSec(fox.speed), dt);
                fra::CancelRootTranslationXZ(mSkinned.skeleton, local);
            }

            if (fox.useLookAt && mHeadJoint >= 0)
            {
                (void) fra::ApplyLookAt(
                    mSkinned.skeleton, local, fox.model,
                    static_cast<std::uint32_t>(mHeadJoint), mCameraPos, 0.75f);
            }

            if (fox.useIk)
            {
                const glm::vec3 pos(fox.model[3]);
                const glm::vec3 fwd = glm::normalize(
                    glm::mat3(fox.model) * glm::vec3(0.f, 0.f, -1.f));
                const float bob =
                    0.12f *
                    std::sin(mAnimClock * 3.f +
                             static_cast<float>(fox.boneOffset) * 0.01f);
                const glm::vec3 target =
                    pos + fwd * 0.55f + glm::vec3(0.f, 0.15f + bob, 0.f);
                const glm::vec3 pole = pos + glm::vec3(0.25f, 0.4f, 0.f);
                (void) fra::SolveTwoBoneIK(mSkinned.skeleton, local, fox.model,
                                           mLegChain, target, pole, 0.85f);
            }

            const auto skin = fra::PoseToSkinMatrices(mSkinned.skeleton, local);
            allBones.insert(allBones.end(), skin.begin(), skin.end());
        }
        if (allBones.empty())
        {
            allBones = fra::PoseToSkinMatrices(
                mSkinned.skeleton, fra::RestLocalPose(mSkinned.skeleton));
        }
        mRenderer->UploadBoneMatrices(allBones);

        const float yawRad   = glm::radians(mYaw);
        const float pitchRad = glm::radians(mPitch);
        glm::vec3   front;
        front.x = std::cos(yawRad) * std::cos(pitchRad);
        front.y = std::sin(pitchRad);
        front.z = std::sin(yawRad) * std::cos(pitchRad);
        front   = glm::normalize(front);
        mRenderer->UpdateCamera(
            mCameraPos, mCameraPos + front, glm::vec3(0.f, 1.f, 0.f));

        std::vector<fra::SceneInstanceUpload> instances;
        instances.reserve(mFoxes.size() * mSkinned.meshIds.size() + 1);
        std::uint32_t entity = 1;
        for (const auto& fox : mFoxes)
        {
            for (const auto meshId : mSkinned.meshIds)
            {
                instances.push_back(fra::SceneInstanceUpload {
                    .model       = fox.model,
                    .meshId      = meshId,
                    .materialId  = mFoxMaterial,
                    .entityId    = entity++,
                    .castShadows = true,
                    .boneOffset  = fox.boneOffset,
                    .boneCount   = jointCount,
                });
            }
        }
        instances.push_back(fra::SceneInstanceUpload {
            .model = glm::scale(
                glm::translate(glm::mat4(1.f), glm::vec3(0.f, -0.05f, 0.f)),
                glm::vec3(120.f, 1.f, 80.f)),
            .meshId      = mGroundMesh,
            .materialId  = mGroundMaterial,
            .entityId    = 100000u,
            .castShadows = false,
        });
        mRenderer->UploadSceneInstances(instances);
        mRenderer->EndFrame();
    }

  private:
    struct FoxActor
    {
        fra::AnimGraph graph;
        glm::mat4      model { 1.f };
        float          speed         = 0.f;
        float          upperTime     = 0.f;
        std::uint32_t  boneOffset    = 0;
        bool           useUpperLayer = false;
        bool           useLookAt     = false;
        bool           useIk         = false;
        bool           useRootMotion = false;
    };

    void setLookHeld(const bool held)
    {
        mLookHeld = held;
        mWindow->SetMouseGrab(held);
    }

    void setSpeed(const float speed)
    {
        mSpeed = std::clamp(speed, 0.f, 3.f);
        for (auto& fox : mFoxes)
        {
            fox.speed         = mSpeed;
            fox.useRootMotion = mSpeed >= 1.5f;
            fox.graph.SetFloat("Speed", mSpeed);
        }
        std::cout << "Speed=" << mSpeed << " (" << mFoxes.size() << " foxes)\n";
    }

    void onKeyPressed(const fra::KeyCode key)
    {
        switch (key)
        {
            case fra::KeyCode::Num1:
                setSpeed(0.f);
                break;
            case fra::KeyCode::Num2:
                setSpeed(1.f);
                break;
            case fra::KeyCode::Num3:
                setSpeed(2.f);
                break;
            case fra::KeyCode::Up:
                setSpeed(mSpeed + 0.25f);
                break;
            case fra::KeyCode::Down:
                setSpeed(mSpeed - 0.25f);
                break;
            default:
                break;
        }
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

    fra::SkinnedModel         mSkinned;
    std::vector<FoxActor>     mFoxes;
    const fra::AnimationClip* mUpperClip = nullptr;
    fra::BoneMask             mUpperMask;
    fra::TwoBoneChain         mLegChain {};
    std::int32_t              mHeadJoint      = -1;
    bool                      mIkReady        = true;
    float                     mAnimClock      = 0.f;
    float                     mSpeed          = 0.f;
    std::uint32_t             mFoxMaterial    = 0;
    std::uint32_t             mGroundMesh     = 0;
    std::uint32_t             mGroundMaterial = 0;

    std::unordered_set<std::uint32_t> mKeysHeld;
    bool                              mLookHeld  = false;
    glm::vec3                         mCameraPos = { 0.f, 28.f, 55.f };
    float                             mYaw       = -90.f;
    float                             mPitch     = -28.f;
};

int main(int, const char**)
{
    const auto app =
        skr::ApplicationBuilder()
            .WithExtension<fra::FreyaExtension>([](fra::FreyaExtension freya) {
                freya.WithOptions([](fra::FreyaOptionsBuilder& freyaOptions) {
                    freyaOptions.SetTitle("SkinnedFox — Rig MVP (look/IK/root)")
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
