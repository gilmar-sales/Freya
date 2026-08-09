#include <Freya/Freya.hpp>
#include <Freya/Vulkan.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <span>
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

    enum class GpuAnimMode : std::uint8_t
    {
        Off,
        Fox0,
        Crowd
    };

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

        // Author footsteps when the asset has no markers (Fox.glb).
        for (auto& clip : mSkinned.clips)
        {
            if (clip.name.find("Walk") != std::string::npos ||
                clip.name.find("Run") != std::string::npos)
                fra::EnsureDefaultFootstepEvents(clip);
        }

        // Re-resolve pointers after mutating clips vector is safe — clips
        // storage is stable; refresh in case Ensure ran on those entries.
        idle = findClip(mSkinned, "Survey");
        walk = findClip(mSkinned, "Walk");
        run  = findClip(mSkinned, "Run");
        if (!idle)
            idle = mSkinned.clips.empty() ? nullptr : &mSkinned.clips[0];
        if (!walk)
            walk = idle;
        if (!run)
            run = walk;

        const float bakeHz = std::max(1.f, mFreyaOptions->animBakeHz);
        mBakeIdle          = fra::BakeClip(mSkinned.skeleton, *idle, bakeHz);
        mBakeWalk          = fra::BakeClip(mSkinned.skeleton, *walk, bakeHz);
        mBakeRun           = fra::BakeClip(mSkinned.skeleton, *run, bakeHz);
        std::cout << "Baked clips @" << bakeHz << "Hz: idle frames="
                  << mBakeIdle.frameCount << " walk=" << mBakeWalk.frameCount
                  << " run=" << mBakeRun.frameCount << '\n';

        mClipIdle = idle;
        mClipWalk = walk;
        mClipRun  = run;

        if (auto* gpu = mRenderer->GetGpuAnimPass())
        {
            gpu->UploadSkeleton(fra::PackSkeleton(mSkinned.skeleton));
            const fra::BakedClip clips[3] = { mBakeIdle, mBakeWalk, mBakeRun };
            gpu->UploadBakes(fra::PackBakedClips(clips));
            gpu->SetCopyPrevBones(false);
            std::cout << "GPU anim bake uploaded (Idle/Walk/Run)\n";
        }

        mUpperClip = idle;
        mUpperMask = makeUpperBodyMask(mSkinned.skeleton);
        mRestPose  = fra::RestLocalPose(mSkinned.skeleton);

        if (auto* gpu = mRenderer->GetGpuAnimPass())
        {
            const auto jc = mSkinned.skeleton.JointCount();
            gpu->UploadBoneMask(fra::PackBoneMask(mUpperMask, jc));
            gpu->UploadRestJoints(fra::PackRestJoints(mRestPose, jc));
            const auto root = fra::FindRootJoint(mSkinned.skeleton);
            gpu->SetRigIndices(
                mHeadJoint >= 0 ? static_cast<std::uint32_t>(mHeadJoint)
                                : 0xffffffffu,
                mLegChain.root, mLegChain.mid, mLegChain.tip,
                root >= 0 ? static_cast<std::uint32_t>(root) : 0xffffffffu);
            std::cout << "GPU anim mask+rest+rig uploaded\n";
        }

        mHeadJoint       = findJointAny(mSkinned.skeleton, { "Head", "head" });
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

        const float   lodMinHz      = fra::AnimLodMinHz(*mFreyaOptions);
        const float   lodStaggerT   = 1.f / std::max(lodMinHz, 1.f);
        std::uint32_t upperCount    = 0;
        std::uint32_t additiveCount = 0;
        std::uint32_t lookCount     = 0;
        std::uint32_t ikCount       = 0;
        std::uint32_t rootCount     = 0;
        mFoxes.reserve(foxCount);
        for (std::uint32_t z = 0; z < kFoxGridZ; ++z)
        {
            for (std::uint32_t x = 0; x < kFoxGridX; ++x)
            {
                const std::uint32_t i = z * kFoxGridX + x;
                FoxActor            fox;
                fox.speed      = speedPreset(i);
                fox.boneOffset = i * jointCount;
                fox.lodAccum =
                    (static_cast<float>(i % 64u) / 64.f) * lodStaggerT;
                fox.useUpperLayer    = (i % 4u) == 0u;
                fox.useAdditiveLayer = (i % 4u) == 2u;
                fox.useLookAt        = mHeadJoint >= 0 && (i % 5u) == 1u;
                fox.useIk            = mIkReady && (i % 13u) == 0u;
                fox.useRootMotion    = fox.speed >= 1.5f;
                if (fox.useUpperLayer)
                    ++upperCount;
                if (fox.useAdditiveLayer)
                    ++additiveCount;
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
                        .AddBlendSample(0.f, *idle, true, 1.f, &mBakeIdle)
                        .AddBlendSample(1.f, *walk, true, 1.f, &mBakeWalk)
                        .AddBlendSample(2.f, *run, true, kRunPlayback,
                                        &mBakeRun)
                        .Entry("Loco")
                        .Build();

                (void) fox.graph.Evaluate(0.17f * static_cast<float>(i % 17));
                fox.upperTime = 0.11f * static_cast<float>(i % 13);
                mFoxes.push_back(std::move(fox));
            }
        }

        std::cout
            << "Anim stack — " << foxCount << " foxes | Blend1D bake@"
            << mFreyaOptions->animBakeHz << "Hz | upper=" << upperCount
            << " additive=" << additiveCount << " look=" << lookCount
            << " ik=" << ikCount << " rootDrive=" << rootCount << '\n'
            << "  1/2/3/Up/Down=Speed   F1=help   F2..F11=feature toggles\n";
        printFeatureHelp();
        printFeatureStatus();

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

        using Clock     = std::chrono::steady_clock;
        using SecondsF  = std::chrono::duration<double>;
        const auto tUp0 = Clock::now();

        mRenderer->BeginFrame();

        const auto             jointCount = mSkinned.skeleton.JointCount();
        std::vector<glm::mat4> allBones;
        const bool             gpuCrowd = mGpuAnimMode == GpuAnimMode::Crowd;
        const bool             gpuFox0  = mGpuAnimMode == GpuAnimMode::Fox0;
        if (!gpuCrowd)
            allBones.reserve(mFoxes.size() * jointCount);
        std::vector<fra::GpuAnimInstance> gpuInstances;
        if (gpuCrowd || gpuFox0)
            gpuInstances.reserve(gpuCrowd ? mFoxes.size() : 1);
        std::vector<fra::FiredAnimationEvent> events;
        events.reserve(8);
        auto&      debugDraw = mRenderer->GetDebugDraw();
        const bool drawDebug =
            mEnableDebugDraw && mRenderer->IsDebugDrawEnabled();
        std::uint32_t foxIndex    = 0;
        std::uint32_t animUpdates = 0;
        double        msAnim      = 0.0;
        double        msSkin      = 0.0;
        const auto    tAnim0      = Clock::now();
        for (auto& fox : mFoxes)
        {
            const glm::vec3 foxPos(fox.model[3]);
            fra::UpdateAnimLodTier(
                *mFreyaOptions, fox.lodTier, glm::length(foxPos - mCameraPos));
            const float hz  = fra::AnimLodHz(*mFreyaOptions, fox.lodTier);
            const bool  due = fra::ConsumeAnimLodTick(fox.lodAccum, dt, hz);
            const bool  forceGpuSeed = gpuCrowd && !mGpuCrowdSeeded;
            const bool  mustEval =
                due || forceGpuSeed ||
                (!gpuCrowd && fox.skinCache.size() != jointCount) ||
                (gpuFox0 && &fox == &mFoxes[0]);

            // Crowd: advance the anim clock every display frame with wall dt
            // so Near/Far share the same playback rate. LOD only gates the
            // GPU skin dispatch (pose may hold between skins).
            if (gpuCrowd && mEnableAnimGraph)
            {
                events.clear();
                fox.graph.Advance(dt, mEnableEvents ? &events : nullptr);
                if (mEnableEvents)
                {
                    for (const auto& e : events)
                    {
                        if (e.name.rfind("Footstep", 0) == 0)
                        {
                            ++fox.footsteps;
                            ++mFootstepTotal;
                        }
                    }
                }
                fox.pendingDt = 0.f;

                const bool tickLayer =
                    (mEnableUpperMask && fox.useUpperLayer) ||
                    (mEnableAdditive && fox.useAdditiveLayer);
                if (tickLayer && mUpperClip)
                {
                    fox.upperTime += dt;
                    if (mUpperClip->duration > 0.f)
                    {
                        fox.upperTime =
                            std::fmod(fox.upperTime, mUpperClip->duration);
                        if (fox.upperTime < 0.f)
                            fox.upperTime += mUpperClip->duration;
                    }
                }
            }
            else
                fox.pendingDt += dt;

            if (mEnableRootMotion && fox.useRootMotion)
            {
                fox.model = fra::DrivePlanarLocomotion(
                    fox.model, locoMetersPerSec(fox.speed), dt);
            }

            if (!mustEval)
            {
                if (!gpuCrowd)
                    allBones.insert(allBones.end(), fox.skinCache.begin(),
                                    fox.skinCache.end());
                ++foxIndex;
                continue;
            }

            ++animUpdates;
            const float stepDt  = fox.pendingDt > 0.f ? fox.pendingDt : dt;
            fox.pendingDt       = 0.f;
            const bool locoOnly = gpuCrowd || (gpuFox0 && &fox == &mFoxes[0]);

            events.clear();
            if (mEnableAnimGraph && !(gpuCrowd && locoOnly))
            {
                // Fox0 golden / CPU: accumulate time on eval ticks.
                if (locoOnly && gpuFox0)
                    fox.graph.Advance(stepDt,
                                      mEnableEvents ? &events : nullptr);
            }

            fra::LocalPose local;
            if (locoOnly && gpuCrowd)
            {
                const fra::AnimationClip* ca = nullptr;
                const fra::AnimationClip* cb = nullptr;
                float                     ta = 0.f, tb = 0.f, bt = 0.f;
                fra::GpuAnimInstance      inst {};
                inst.boneOffset = fox.boneOffset;
                inst.jointCount = jointCount;
                inst.flags      = fra::GpuAnimFlags::Loop;
                if (fox.graph.TryGetBlend1DGpuSample(ca, ta, cb, tb, bt))
                {
                    inst.clipA  = gpuClipIndex(ca);
                    inst.clipB  = gpuClipIndex(cb);
                    inst.timeA  = ta;
                    inst.timeB  = tb;
                    inst.blendT = bt;
                }
                if (mEnableUpperMask && fox.useUpperLayer && mUpperClip)
                {
                    inst.flags |= fra::GpuAnimFlags::MaskedOverlay;
                    inst.clipLayer   = gpuClipIndex(mUpperClip);
                    inst.maskBase    = 0;
                    inst.timeLayer   = fox.upperTime;
                    inst.layerWeight = 0.85f;
                }
                else if (mEnableAdditive && fox.useAdditiveLayer && mUpperClip)
                {
                    inst.flags |= fra::GpuAnimFlags::Additive;
                    inst.clipLayer   = gpuClipIndex(mUpperClip);
                    inst.maskBase    = 0;
                    inst.timeLayer   = fox.upperTime;
                    inst.layerWeight = 0.55f;
                }
                inst.modelWorld = fox.model;
                if (mEnableRootMotion && fox.useRootMotion)
                    inst.flags |= fra::GpuAnimFlags::CancelRootXZ;
                if (mEnableLookAt && fox.useLookAt && mHeadJoint >= 0)
                {
                    inst.lookTarget = mCameraPos;
                    inst.lookWeight = 0.75f;
                }
                if (mEnableIk && fox.useIk && mIkReady)
                {
                    const glm::vec3 pos(fox.model[3]);
                    const glm::vec3 fwd = glm::normalize(
                        glm::mat3(fox.model) * glm::vec3(0.f, 0.f, -1.f));
                    const float bob =
                        0.12f *
                        std::sin(mAnimClock * 3.f +
                                 static_cast<float>(fox.boneOffset) * 0.01f);
                    inst.ikTarget =
                        pos + fwd * 0.55f + glm::vec3(0.f, 0.15f + bob, 0.f);
                    inst.ikPole   = pos + glm::vec3(0.25f, 0.4f, 0.f);
                    inst.ikWeight = 0.85f;
                }
                gpuInstances.push_back(inst);

                if (drawDebug && foxIndex < 12u)
                {
                    local = fox.graph.SampleCurrent();
                    debugDraw.DrawSkeleton(mSkinned.skeleton, local, fox.model,
                                           { 0.2f, 0.95f, 0.45f, 0.9f });
                }
                ++foxIndex;
                continue;
            }

            if (mEnableAnimGraph)
            {
                if (!(locoOnly && gpuFox0))
                    local = fox.graph.Evaluate(
                        stepDt, mEnableEvents ? &events : nullptr);
                else
                    local = fox.graph.SampleCurrent();
            }
            else
                local = fra::RestLocalPose(mSkinned.skeleton);

            if (mEnableEvents)
            {
                for (const auto& e : events)
                {
                    if (e.name.rfind("Footstep", 0) == 0)
                    {
                        ++fox.footsteps;
                        ++mFootstepTotal;
                    }
                }
            }

            if (!locoOnly && mEnableUpperMask && fox.useUpperLayer &&
                mUpperClip)
            {
                fox.upperTime += stepDt;
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
            else if (!locoOnly && mEnableAdditive && fox.useAdditiveLayer &&
                     mUpperClip)
            {
                fox.upperTime += stepDt;
                if (mUpperClip->duration > 0.f)
                {
                    fox.upperTime =
                        std::fmod(fox.upperTime, mUpperClip->duration);
                    if (fox.upperTime < 0.f)
                        fox.upperTime += mUpperClip->duration;
                }
                const auto add = fra::SampleClip(
                    mSkinned.skeleton, *mUpperClip, fox.upperTime, true);
                local = fra::BlendAdditive(
                    local, add, mRestPose, mUpperMask, 0.55f);
            }

            if (!locoOnly && mEnableRootMotion && fox.useRootMotion)
                fra::CancelRootTranslationXZ(mSkinned.skeleton, local);

            const bool doLook =
                !locoOnly && mEnableLookAt && fox.useLookAt && mHeadJoint >= 0;
            if (doLook)
            {
                (void) fra::ApplyLookAt(
                    mSkinned.skeleton, local, fox.model,
                    static_cast<std::uint32_t>(mHeadJoint), mCameraPos, 0.75f);
            }

            glm::vec3  ikTarget {};
            glm::vec3  ikPole {};
            const bool doIk = !locoOnly && mEnableIk && fox.useIk;
            if (doIk)
            {
                const glm::vec3 pos(fox.model[3]);
                const glm::vec3 fwd = glm::normalize(
                    glm::mat3(fox.model) * glm::vec3(0.f, 0.f, -1.f));
                const float bob =
                    0.12f *
                    std::sin(mAnimClock * 3.f +
                             static_cast<float>(fox.boneOffset) * 0.01f);
                ikTarget = pos + fwd * 0.55f + glm::vec3(0.f, 0.15f + bob, 0.f);
                ikPole   = pos + glm::vec3(0.25f, 0.4f, 0.f);
                (void) fra::SolveTwoBoneIK(mSkinned.skeleton, local, fox.model,
                                           mLegChain, ikTarget, ikPole, 0.85f);
            }

            if (drawDebug && (doLook || doIk || foxIndex < 12u))
            {
                debugDraw.DrawSkeleton(mSkinned.skeleton, local, fox.model,
                                       { 0.2f, 0.95f, 0.45f, 0.9f });
                if (doLook)
                {
                    debugDraw.DrawLookRay(
                        mSkinned.skeleton, local, fox.model,
                        static_cast<std::uint32_t>(mHeadJoint), mCameraPos,
                        { 0.3f, 0.85f, 1.f, 1.f });
                }
                if (doIk)
                {
                    debugDraw.DrawTwoBoneIk(
                        mSkinned.skeleton, local, fox.model, mLegChain.root,
                        mLegChain.mid, mLegChain.tip, ikTarget, ikPole,
                        { 1.f, 0.55f, 0.15f, 1.f }, { 1.f, 0.2f, 0.35f, 1.f });
                }
            }
            ++foxIndex;

            if (gpuFox0 && &fox == &mFoxes[0])
            {
                const fra::AnimationClip* ca = nullptr;
                const fra::AnimationClip* cb = nullptr;
                float                     ta = 0.f, tb = 0.f, bt = 0.f;
                fra::GpuAnimInstance      inst {};
                inst.boneOffset = fox.boneOffset;
                inst.jointCount = jointCount;
                inst.flags      = fra::GpuAnimFlags::Loop;
                if (fox.graph.TryGetBlend1DGpuSample(ca, ta, cb, tb, bt))
                {
                    inst.clipA  = gpuClipIndex(ca);
                    inst.clipB  = gpuClipIndex(cb);
                    inst.timeA  = ta;
                    inst.timeB  = tb;
                    inst.blendT = bt;
                }
                gpuInstances.push_back(inst);
            }

            const auto tSkin0 = Clock::now();
            fox.skinCache = fra::PoseToSkinMatrices(mSkinned.skeleton, local);
            allBones.insert(allBones.end(), fox.skinCache.begin(),
                            fox.skinCache.end());
            msSkin += SecondsF(Clock::now() - tSkin0).count() * 1000.0;
            if (gpuFox0 && &fox == &mFoxes[0])
                mGpuFox0CpuSkin = fox.skinCache;
        }
        msAnim = SecondsF(Clock::now() - tAnim0).count() * 1000.0 - msSkin;
        mProfAnimUpdates += animUpdates;

        mFootstepLogTimer += dt;
        if (mEnableEvents && mFootstepLogTimer >= 2.f)
        {
            mFootstepLogTimer = 0.f;
            std::cout << "Footsteps total=" << mFootstepTotal << '\n';
        }

        const auto tBone0 = Clock::now();
        if (!gpuCrowd)
        {
            if (allBones.empty())
            {
                allBones = fra::PoseToSkinMatrices(
                    mSkinned.skeleton, fra::RestLocalPose(mSkinned.skeleton));
            }
            mRenderer->UploadBoneMatrices(allBones);
        }
        const double msBoneUpload =
            SecondsF(Clock::now() - tBone0).count() * 1000.0;

        if (auto* gpu = mRenderer->GetGpuAnimPass())
        {
            if (!gpuInstances.empty())
            {
                gpu->SetCopyPrevBones(gpuCrowd);
                gpu->UploadInstances(gpuInstances);
                gpu->SetEnabled(true);
                if (gpuCrowd && gpuInstances.size() == mFoxes.size())
                    mGpuCrowdSeeded = true;
            }
            else
            {
                gpu->SetEnabled(false);
                gpu->UploadInstances({});
            }
        }

        const float yawRad   = glm::radians(mYaw);
        const float pitchRad = glm::radians(mPitch);
        glm::vec3   front;
        front.x = std::cos(yawRad) * std::cos(pitchRad);
        front.y = std::sin(pitchRad);
        front.z = std::sin(yawRad) * std::cos(pitchRad);
        front   = glm::normalize(front);
        mRenderer->UpdateCamera(
            mCameraPos, mCameraPos + front, glm::vec3(0.f, 1.f, 0.f));

        const auto                            tInst0 = Clock::now();
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
                    .castShadows = mEnableShadows,
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
        const double msInstances =
            SecondsF(Clock::now() - tInst0).count() * 1000.0;

        const auto tEnd0       = Clock::now();
        const auto gpuFrameIdx = mRenderer->GetCurrentFrameIndex();
        mRenderer->EndFrame();
        const double msEndFrame =
            SecondsF(Clock::now() - tEnd0).count() * 1000.0;
        const double msUpdate = SecondsF(Clock::now() - tUp0).count() * 1000.0;

        if (mGpuAnimGoldenOnce && mGpuAnimMode == GpuAnimMode::Fox0 &&
            !mFoxes.empty() && !mGpuFox0CpuSkin.empty())
        {
            mGpuAnimGoldenOnce = false;
            std::vector<glm::mat4> gpuBones(jointCount);
            if (mRenderer->ReadbackGpuAnimBones(
                    gpuFrameIdx, mFoxes[0].boneOffset, gpuBones))
            {
                float maxAbs = 0.f;
                for (std::uint32_t i = 0; i < jointCount; ++i)
                {
                    const auto d = gpuBones[i] - mGpuFox0CpuSkin[i];
                    for (int c = 0; c < 4; ++c)
                        for (int r = 0; r < 4; ++r)
                            maxAbs = std::max(maxAbs, std::abs(d[c][r]));
                }
                std::cout << "GPU golden fox0 maxAbsDiff=" << maxAbs << '\n';
            }
            else
                std::cout << "GPU golden readback failed\n";
        }

        ++mProfFrames;
        mProfAnimMs += msAnim;
        mProfSkinMs += msSkin;
        mProfBoneMs += msBoneUpload;
        mProfInstMs += msInstances;
        mProfEndMs += msEndFrame;
        mProfUpdateMs += msUpdate;
        mProfDtSum += dt;
        mProfReportTimer += dt;
        if (mProfReportTimer >= 1.f && mProfFrames > 0)
        {
            const double n = static_cast<double>(mProfFrames);
            const double fps =
                mProfDtSum > 0.0 ? static_cast<double>(mProfFrames) / mProfDtSum
                                 : 0.0;
            std::cout << "CPU avg  fps=" << fps << "  anim="
                      << (mProfAnimMs / n) << "ms  skin=" << (mProfSkinMs / n)
                      << "ms  boneUp=" << (mProfBoneMs / n)
                      << "ms  instUp=" << (mProfInstMs / n)
                      << "ms  endFrame=" << (mProfEndMs / n) << "ms  update="
                      << (mProfUpdateMs / n) << "ms  (foxes=" << mFoxes.size()
                      << " animTicks=" << (mProfAnimUpdates / n)
                      << " inst=" << instances.size()
                      << " bones=" << allBones.size() << ")\n";
            mProfReportTimer = 0.f;
            mProfFrames      = 0;
            mProfAnimMs = mProfSkinMs = mProfBoneMs = 0.0;
            mProfInstMs = mProfEndMs = mProfUpdateMs = 0.0;
            mProfDtSum                               = 0.0;
            mProfAnimUpdates                         = 0;
        }
    }

  private:
    struct FoxActor
    {
        fra::AnimGraph         graph;
        glm::mat4              model { 1.f };
        std::vector<glm::mat4> skinCache;
        float                  speed            = 0.f;
        float                  upperTime        = 0.f;
        float                  pendingDt        = 0.f;
        float                  lodAccum         = 0.f;
        std::uint32_t          boneOffset       = 0;
        std::uint32_t          footsteps        = 0;
        std::uint8_t           lodTier          = 0;
        bool                   useUpperLayer    = false;
        bool                   useAdditiveLayer = false;
        bool                   useLookAt        = false;
        bool                   useIk            = false;
        bool                   useRootMotion    = false;
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

    static const char* onOff(const bool v) { return v ? "ON " : "off"; }

    static const char* animQualityName(const fra::AnimationQuality q)
    {
        switch (q)
        {
            case fra::AnimationQuality::Low:
                return "Low";
            case fra::AnimationQuality::Medium:
                return "Medium";
            case fra::AnimationQuality::High:
                return "High";
            case fra::AnimationQuality::Ultra:
                return "Ultra";
            case fra::AnimationQuality::Off:
                return "Off";
        }
        return "?";
    }

    void printFeatureHelp() const
    {
        std::cout << "Feature toggles:\n"
                  << "  F1  help/status\n"
                  << "  F2  cast shadows (foxes)\n"
                  << "  F3  debug draw\n"
                  << "  F4  AnimGraph evaluate (off = rest pose)\n"
                  << "  F5  upper BlendMasked layer\n"
                  << "  F6  additive upper layer\n"
                  << "  F7  look-at\n"
                  << "  F8  two-bone IK\n"
                  << "  F9  root/locomotion drive\n"
                  << "  F10 clip events / footstep log\n"
                  << "  F11 cycle AnimationQuality "
                     "(Low/Med/High/Ultra/Off)\n"
                  << "  F12 GPU anim: Off → Fox0 golden → Crowd\n"
                  << "Loco Blend1D uses shared bake @"
                  << mFreyaOptions->animBakeHz
                  << "Hz; LOD rates are wall-clock Hz (F11)\n"
                  << "CPU avg line every 1s: anim/skin/boneUp/instUp/"
                     "endFrame/update + animTicks\n";
    }

    void printFeatureStatus() const
    {
        const auto& o = *mFreyaOptions;
        std::cout << "Features  shadow=" << onOff(mEnableShadows)
                  << " debug=" << onOff(mEnableDebugDraw)
                  << " graph=" << onOff(mEnableAnimGraph)
                  << " mask=" << onOff(mEnableUpperMask) << " add="
                  << onOff(mEnableAdditive) << " look=" << onOff(mEnableLookAt)
                  << " ik=" << onOff(mEnableIk)
                  << " root=" << onOff(mEnableRootMotion)
                  << " events=" << onOff(mEnableEvents)
                  << " animQ=" << animQualityName(mAnimationQuality)
                  << " lod=" << onOff(o.enableAnimLod) << '\n'
                  << "  lodHz=" << o.animLodHz[0] << '/' << o.animLodHz[1]
                  << '/' << o.animLodHz[2] << '/' << o.animLodHz[3]
                  << " exitDist=" << o.animLodExitDist[0] << '/'
                  << o.animLodExitDist[1] << '/' << o.animLodExitDist[2]
                  << '\n';
    }

    void setAnimationQuality(const fra::AnimationQuality quality)
    {
        mAnimationQuality = quality;
        fra::ApplyAnimationQuality(*mFreyaOptions, quality);
        const float staggerT =
            1.f / std::max(fra::AnimLodMinHz(*mFreyaOptions), 1.f);
        for (std::uint32_t i = 0; i < mFoxes.size(); ++i)
            mFoxes[i].lodAccum =
                (static_cast<float>(i % 64u) / 64.f) * staggerT;
        std::cout << "AnimationQuality " << animQualityName(quality) << '\n';
        printFeatureStatus();
    }

    void cycleAnimationQuality()
    {
        using Q = fra::AnimationQuality;
        switch (mAnimationQuality)
        {
            case Q::Low:
                setAnimationQuality(Q::Medium);
                break;
            case Q::Medium:
                setAnimationQuality(Q::High);
                break;
            case Q::High:
                setAnimationQuality(Q::Ultra);
                break;
            case Q::Ultra:
                setAnimationQuality(Q::Off);
                break;
            case Q::Off:
                setAnimationQuality(Q::Low);
                break;
        }
    }

    void toggle(bool& flag, const char* name)
    {
        flag = !flag;
        if (&flag == &mEnableDebugDraw)
            mRenderer->SetDebugDrawEnabled(flag);
        std::cout << name << ' ' << (flag ? "ON" : "OFF") << '\n';
        printFeatureStatus();
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
            case fra::KeyCode::F1:
                printFeatureHelp();
                printFeatureStatus();
                break;
            case fra::KeyCode::F2:
                toggle(mEnableShadows, "Shadows");
                break;
            case fra::KeyCode::F3:
                toggle(mEnableDebugDraw, "DebugDraw");
                break;
            case fra::KeyCode::F4:
                toggle(mEnableAnimGraph, "AnimGraph");
                break;
            case fra::KeyCode::F5:
                toggle(mEnableUpperMask, "UpperMask");
                break;
            case fra::KeyCode::F6:
                toggle(mEnableAdditive, "Additive");
                break;
            case fra::KeyCode::F7:
                toggle(mEnableLookAt, "LookAt");
                break;
            case fra::KeyCode::F8:
                toggle(mEnableIk, "IK");
                break;
            case fra::KeyCode::F9:
                toggle(mEnableRootMotion, "RootMotion");
                break;
            case fra::KeyCode::F10:
                toggle(mEnableEvents, "Events");
                break;
            case fra::KeyCode::F11:
                cycleAnimationQuality();
                break;
            case fra::KeyCode::F12:
                cycleGpuAnimMode();
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
    fra::BakedClip            mBakeIdle;
    fra::BakedClip            mBakeWalk;
    fra::BakedClip            mBakeRun;
    const fra::AnimationClip* mClipIdle = nullptr;
    const fra::AnimationClip* mClipWalk = nullptr;
    const fra::AnimationClip* mClipRun  = nullptr;
    std::vector<FoxActor>     mFoxes;
    const fra::AnimationClip* mUpperClip = nullptr;
    fra::BoneMask             mUpperMask;
    fra::LocalPose            mRestPose;
    fra::TwoBoneChain         mLegChain {};
    std::int32_t              mHeadJoint         = -1;
    bool                      mIkReady           = true;
    bool                      mEnableShadows     = true;
    bool                      mEnableDebugDraw   = false;
    bool                      mEnableAnimGraph   = true;
    bool                      mEnableUpperMask   = true;
    bool                      mEnableAdditive    = true;
    bool                      mEnableLookAt      = true;
    bool                      mEnableIk          = true;
    bool                      mEnableRootMotion  = true;
    bool                      mEnableEvents      = true;
    fra::AnimationQuality     mAnimationQuality  = fra::AnimationQuality::High;
    GpuAnimMode               mGpuAnimMode       = GpuAnimMode::Off;
    bool                      mGpuAnimGoldenOnce = false;
    bool                      mGpuCrowdSeeded    = false;
    std::vector<glm::mat4>    mGpuFox0CpuSkin;

    void cycleGpuAnimMode()
    {
        switch (mGpuAnimMode)
        {
            case GpuAnimMode::Off:
                mGpuAnimMode       = GpuAnimMode::Fox0;
                mGpuAnimGoldenOnce = true;
                std::cout << "GpuAnim Fox0 (+golden)\n";
                break;
            case GpuAnimMode::Fox0:
                mGpuAnimMode    = GpuAnimMode::Crowd;
                mGpuCrowdSeeded = false;
                std::cout << "GpuAnim Crowd (LOD + layers/look/IK on GPU)\n";
                break;
            case GpuAnimMode::Crowd:
                mGpuAnimMode = GpuAnimMode::Off;
                if (auto* gpu = mRenderer->GetGpuAnimPass())
                {
                    gpu->SetEnabled(false);
                    gpu->UploadInstances({});
                }
                std::cout << "GpuAnim OFF (CPU)\n";
                break;
        }
    }

    [[nodiscard]] std::uint32_t gpuClipIndex(
        const fra::AnimationClip* clip) const
    {
        if (clip == mClipWalk)
            return 1u;
        if (clip == mClipRun)
            return 2u;
        return 0u;
    }

    float         mProfReportTimer  = 0.f;
    std::uint32_t mProfFrames       = 0;
    std::uint32_t mProfAnimUpdates  = 0;
    double        mProfAnimMs       = 0.0;
    double        mProfSkinMs       = 0.0;
    double        mProfBoneMs       = 0.0;
    double        mProfInstMs       = 0.0;
    double        mProfEndMs        = 0.0;
    double        mProfUpdateMs     = 0.0;
    double        mProfDtSum        = 0.0;
    float         mAnimClock        = 0.f;
    float         mSpeed            = 0.f;
    float         mFootstepLogTimer = 0.f;
    std::uint64_t mFootstepTotal    = 0;
    std::uint32_t mFoxMaterial      = 0;
    std::uint32_t mGroundMesh       = 0;
    std::uint32_t mGroundMaterial   = 0;

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
                    freyaOptions
                        .SetTitle("SkinnedFox — F1 help / F2-F12 toggles")
                        .SetWidth(1600)
                        .SetHeight(900)
                        .SetVSync(false)
                        .WithReverseZ()
                        .SetIblIntensity(0.2f)
                        .SetShadowQuality(fra::ShadowQuality::Medium)
                        .SetAnimationQuality(fra::AnimationQuality::High)
                        .SetFullscreen(false);
                });
            })
            .Build<MainApp>();

    app->Run();
    return 0;
}
