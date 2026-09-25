#include "Freya/Internal/GpuAnimPassImpl.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <string>

namespace FREYA_NAMESPACE
{
    GpuAnimPass::Impl::Impl(
        const skr::Arc<Device>&              device,
        const skr::Arc<BoneMatrixResources>& boneResources,
        const vk::PipelineLayout             pipelineLayout,
        const vk::Pipeline                   pipeline,
        const vk::DescriptorSetLayout        animSetLayout,
        const vk::DescriptorPool             animPool,
        const vk::DescriptorSet              animSet,
        const skr::Arc<Buffer>&              parentsBuffer,
        const skr::Arc<Buffer>&              invBindBuffer,
        const skr::Arc<Buffer>&              skeletonHeaderBuffer,
        const skr::Arc<Buffer>&              clipHeaderBuffer,
        const skr::Arc<Buffer>&              jointsBuffer,
        const skr::Arc<Buffer>&              instanceBuffer,
        const skr::Arc<Buffer>&              boneMaskBuffer,
        const skr::Arc<Buffer>&              restJointsBuffer,
        const skr::Arc<Buffer>&              localScratchBuffer,
        const skr::Arc<Buffer>&              globalScratchBuffer,
        const skr::Arc<Buffer>&              readbackBuffer,
        const skr::Arc<Buffer>&              extractRingBuffer,
        const std::uint32_t                  frameCount,
        const bool                           quantizedJoints) :
        mDevice(device), mBoneResources(boneResources),
        mPipelineLayout(pipelineLayout), mPipeline(pipeline),
        mAnimSetLayout(animSetLayout), mAnimPool(animPool), mAnimSet(animSet),
        mParentsBuffer(parentsBuffer), mInvBindBuffer(invBindBuffer),
        mSkeletonHeaderBuffer(skeletonHeaderBuffer),
        mClipHeaderBuffer(clipHeaderBuffer), mJointsBuffer(jointsBuffer),
        mInstanceBuffer(instanceBuffer), mBoneMaskBuffer(boneMaskBuffer),
        mRestJointsBuffer(restJointsBuffer),
        mLocalScratchBuffer(localScratchBuffer),
        mGlobalScratchBuffer(globalScratchBuffer),
        mReadbackBuffer(readbackBuffer), mExtractRingBuffer(extractRingBuffer),
        mQuantizedJoints(quantizedJoints), mFrameCount(std::max(1u, frameCount))
    {
        mExtractMeta.resize(mFrameCount);
        mExtractCounts.resize(mFrameCount, 0);
        mExtractValid.resize(mFrameCount, 0);
        mExtractSourceFrame.resize(mFrameCount, 0);
        mTimingPending.resize(mFrameCount, 0);
        mTimingHasCarry.resize(mFrameCount, 0);
        mTimingInstanceCount.resize(mFrameCount, 0);
        mTimingSourceFrame.resize(mFrameCount, 0);
        createTimestampPool();
    }

    void GpuAnimPass::Impl::createTimestampPool()
    {
        mTimestampPool     = nullptr;
        mTimestampPeriodNs = 0.f;
        if (!mDevice)
            return;

        auto physical = mDevice->GetPhysicalDevice();
        if (!physical)
            return;

        const auto props = physical->Get().getProperties();
        if (props.limits.timestampPeriod <= 0.f)
            return;

        const auto family = mDevice->GetQueueFamilyIndices().graphicsFamily;
        if (!family)
            return;
        const auto qProps = physical->Get().getQueueFamilyProperties();
        if (*family >= qProps.size() || qProps[*family].timestampValidBits == 0)
            return;

        const auto queryCount =
            mFrameCount * GpuAnimPass::kTimestampQueriesPerSlot;
        try
        {
            mTimestampPool = mDevice->Get().createQueryPool(
                vk::QueryPoolCreateInfo()
                    .setQueryType(vk::QueryType::eTimestamp)
                    .setQueryCount(queryCount));
            mTimestampPeriodNs = props.limits.timestampPeriod;
        }
        catch (const vk::SystemError&)
        {
            mTimestampPool     = nullptr;
            mTimestampPeriodNs = 0.f;
        }
    }

    void GpuAnimPass::Impl::writeTimestamp(
        const vk::CommandBuffer         commandBuffer,
        const std::uint32_t             queryIndex,
        const vk::PipelineStageFlagBits stage) const
    {
        if (!mTimestampPool)
            return;
        commandBuffer.writeTimestamp(stage, mTimestampPool, queryIndex);
    }

    GpuAnimPass::Impl::~Impl()
    {
        if (!mDevice)
            return;

        mDevice->Get().waitIdle();
        if (mTimestampPool)
        {
            mDevice->Get().destroyQueryPool(mTimestampPool);
            mTimestampPool = nullptr;
        }
        if (mPipeline)
            mDevice->Get().destroyPipeline(mPipeline);
        if (mPipelineLayout)
            mDevice->Get().destroyPipelineLayout(mPipelineLayout);
        if (mAnimPool)
            mDevice->Get().destroyDescriptorPool(mAnimPool);
        if (mAnimSetLayout)
            mDevice->Get().destroyDescriptorSetLayout(mAnimSetLayout);
    }

    void GpuAnimPass::Impl::SetJointExtractList(
        const std::span<const GpuJointExtractRequest> reqs)
    {
        const auto n = std::min(static_cast<std::uint32_t>(reqs.size()),
                                GpuAnimPass::kMaxExtractJoints);
        mExtractRequests.assign(reqs.begin(), reqs.begin() + n);
    }

    bool GpuAnimPass::Impl::PollJointExtract(
        const std::uint32_t frameIndex,
        const std::span<GpuJointExtractSample>
                       out,
        std::uint32_t* outCount) const
    {
        if (outCount)
            *outCount = 0;
        if (!mExtractRingBuffer || mFrameCount == 0 || out.empty())
            return false;

        // Previous finished frame in the FiF ring (N+1 when polled at Update
        // start before this slot is overwritten by Dispatch).
        const auto prev =
            (extractSlot(frameIndex) + mFrameCount - 1u) % mFrameCount;
        if (!mExtractValid[prev] || mExtractCounts[prev] == 0)
            return false;

        const auto n = std::min(
            { mExtractCounts[prev], static_cast<std::uint32_t>(out.size()),
              static_cast<std::uint32_t>(mExtractMeta[prev].size()) });
        if (n == 0)
            return false;

        const void* mapped = mExtractRingBuffer->GetMapped();
        if (!mapped)
            return false;

        const auto* mats = reinterpret_cast<const glm::mat4*>(
            static_cast<const std::uint8_t*>(mapped) +
            static_cast<std::size_t>(prev) * extractSlotBytes());

        for (std::uint32_t i = 0; i < n; ++i)
        {
            out[i].boneOffset  = mExtractMeta[prev][i].boneOffset;
            out[i].jointIndex  = mExtractMeta[prev][i].jointIndex;
            out[i].sourceFrame = mExtractSourceFrame[prev];
            out[i]._pad        = 0;
            out[i].skinMatrix  = mats[i];
        }
        if (outCount)
            *outCount = n;
        return true;
    }

    bool GpuAnimPass::Impl::PollTiming(const std::uint32_t  frameIndex,
                                       GpuAnimTimingSample& out) const
    {
        out = {};
        if (!mTimestampPool || mTimestampPeriodNs <= 0.f || mFrameCount == 0)
            return false;

        const auto prev =
            (extractSlot(frameIndex) + mFrameCount - 1u) % mFrameCount;
        if (!mTimingPending[prev])
            return false;

        std::array<std::uint64_t, GpuAnimPass::kTimestampQueriesPerSlot>
                   stamps {};
        const auto first  = prev * GpuAnimPass::kTimestampQueriesPerSlot;
        const auto result = mDevice->Get().getQueryPoolResults(
            mTimestampPool, first, GpuAnimPass::kTimestampQueriesPerSlot,
            sizeof(stamps), stamps.data(), sizeof(std::uint64_t),
            vk::QueryResultFlagBits::e64);
        if (result != vk::Result::eSuccess)
            return false;

        auto deltaMs = [&](const std::uint64_t a, const std::uint64_t b) {
            if (b < a)
                return 0.f;
            return static_cast<float>(b - a) * mTimestampPeriodNs * 1.0e-6f;
        };

        out.valid         = true;
        out.hasCarry      = mTimingHasCarry[prev] != 0;
        out.carryMs       = out.hasCarry ? deltaMs(stamps[0], stamps[1]) : 0.f;
        out.bakeMs        = deltaMs(stamps[2], stamps[3]);
        out.instanceCount = mTimingInstanceCount[prev];
        out.sourceFrame   = mTimingSourceFrame[prev];
        return true;
    }

    void GpuAnimPass::Impl::SetRigIndices(
        const std::uint32_t lookJoint, const std::uint32_t ikRoot,
        const std::uint32_t ikMid, const std::uint32_t ikTip,
        const std::uint32_t rootJoint, const glm::vec3 lookLocalForward,
        const float lookMaxYawRad, const float lookMaxPitchRad)
    {
        mLookJoint        = lookJoint;
        mIkRoot           = ikRoot;
        mIkMid            = ikMid;
        mIkTip            = ikTip;
        mRootJoint        = rootJoint;
        mLookLocalForward = lookLocalForward;
        mLookMaxYawRad    = lookMaxYawRad;
        mLookMaxPitchRad  = lookMaxPitchRad;

        // Back-compat: CancelRootXZ for single-rig slot 0.
        const SpinLockGuard guard(mSkeletonCacheLock);
        if (mSkeletonSlots[0].resident)
        {
            mSkeletonSlots[0].rootJoint = rootJoint;
            WriteSkeletonHeaderUnlocked(0, mSkeletonSlots[0].joints, rootJoint);
        }
    }

    void GpuAnimPass::Impl::WriteSkeletonHeaderUnlocked(
        const std::uint32_t slot, const std::uint32_t jointCount,
        const std::uint32_t rootJoint)
    {
        if (!mSkeletonHeaderBuffer || slot >= GpuAnimPass::kMaxSkeletons)
            return;
        GpuSkeletonHeader h {};
        h.jointCount = jointCount;
        h.rootJoint  = rootJoint;
        mSkeletonHeaderBuffer->Copy(
            &h, sizeof(GpuSkeletonHeader),
            static_cast<std::uint64_t>(slot) * sizeof(GpuSkeletonHeader));
    }

    void GpuAnimPass::Impl::UploadSkeleton(const GpuSkeletonPack& skeleton)
    {
        assert(!mInstanceStagingOpen &&
               "UploadSkeleton during instance staging");
        if (mInstanceStagingOpen)
            return;

        const SpinLockGuard guard(mSkeletonCacheLock);
        // Slot 0 pinned anonymous key — single-rig UploadSkeleton path.
        constexpr std::uint64_t kSlot0Key = 1ull;
        if (!UploadSkeletonSlotUnlocked(0, kSlot0Key, skeleton, mRootJoint))
            return;
        mSkeletonSlots[0].pinned = true;
        mJointCount              = mSkeletonSlots[0].joints;
    }

    void GpuAnimPass::Impl::TouchSkeletonSlotUnlocked(const std::uint32_t slot)
    {
        if (slot >= GpuAnimPass::kMaxSkeletons ||
            !mSkeletonSlots[slot].resident)
            return;
        mSkeletonSlots[slot].lastTouch = ++mSkeletonTouchClock;
    }

    void GpuAnimPass::Impl::EvictSkeletonSlotUnlocked(const std::uint32_t slot)
    {
        if (slot >= GpuAnimPass::kMaxSkeletons)
            return;
        mSkeletonSlots[slot] = {};
        WriteSkeletonHeaderUnlocked(slot, 0, 0xffffffffu);
        if (slot == 0)
            mJointCount = 0;
    }

    bool GpuAnimPass::Impl::UploadSkeletonSlotUnlocked(
        const std::uint32_t slot, const std::uint64_t key,
        const GpuSkeletonPack& skeleton, const std::uint32_t rootJoint)
    {
        if (slot >= GpuAnimPass::kMaxSkeletons || key == 0 || !mParentsBuffer ||
            !mInvBindBuffer)
            return false;

        const auto jc = std::min(skeleton.jointCount, GpuAnimPass::kMaxJoints);
        if (jc == 0)
            return false;

        const auto skelBase = slot * GpuAnimPass::kMaxJoints;
        const auto resolvedRoot =
            rootJoint != 0xffffffffu ? rootJoint : mRootJoint;

        std::vector<std::int32_t> parents(GpuAnimPass::kMaxJoints, -1);
        for (std::uint32_t i = 0; i < jc; ++i)
        {
            if (i < skeleton.parents.size())
                parents[i] = skeleton.parents[i];
        }
        mParentsBuffer->Copy(
            parents.data(),
            static_cast<std::uint32_t>(parents.size() * sizeof(std::int32_t)),
            static_cast<std::uint64_t>(skelBase) * sizeof(std::int32_t));

        std::vector<glm::mat4> inv(GpuAnimPass::kMaxJoints, glm::mat4(1.f));
        for (std::uint32_t i = 0; i < jc; ++i)
        {
            if (i < skeleton.inverseBind.size())
                inv[i] = skeleton.inverseBind[i];
        }
        mInvBindBuffer->Copy(
            inv.data(),
            static_cast<std::uint32_t>(inv.size() * sizeof(glm::mat4)),
            static_cast<std::uint64_t>(skelBase) * sizeof(glm::mat4));

        WriteSkeletonHeaderUnlocked(slot, jc, resolvedRoot);

        auto& meta     = mSkeletonSlots[slot];
        meta.key       = key;
        meta.resident  = true;
        meta.pinned    = false;
        meta.joints    = jc;
        meta.rootJoint = resolvedRoot;
        meta.lastTouch = ++mSkeletonTouchClock;
        return true;
    }

    std::uint32_t GpuAnimPass::Impl::FindSkeletonSlotUnlocked(
        const std::uint64_t key) const
    {
        if (key == 0)
            return 0xffffffffu;
        for (std::uint32_t i = 0; i < GpuAnimPass::kMaxSkeletons; ++i)
        {
            if (mSkeletonSlots[i].resident && mSkeletonSlots[i].key == key)
                return i;
        }
        return 0xffffffffu;
    }

    std::uint32_t GpuAnimPass::Impl::FindSkeletonSlot(
        const std::uint64_t key) const
    {
        const SpinLockGuard guard(mSkeletonCacheLock);
        return FindSkeletonSlotUnlocked(key);
    }

    std::uint32_t GpuAnimPass::Impl::EnsureSkeletonResident(
        const std::uint64_t key, const GpuSkeletonPack& skeleton,
        const std::uint32_t rootJoint)
    {
        const SpinLockGuard guard(mSkeletonCacheLock);
        const auto          hit = FindSkeletonSlotUnlocked(key);
        if (hit != 0xffffffffu)
        {
            TouchSkeletonSlotUnlocked(hit);
            return hit;
        }

        std::uint32_t freeSlot = 0xffffffffu;
        for (std::uint32_t i = 0; i < GpuAnimPass::kMaxSkeletons; ++i)
        {
            if (!mSkeletonSlots[i].resident)
            {
                freeSlot = i;
                break;
            }
        }

        if (freeSlot == 0xffffffffu)
        {
            if (mInstanceStagingOpen)
                return 0xffffffffu;

            std::uint64_t oldest = ~0ull;
            for (std::uint32_t i = 0; i < GpuAnimPass::kMaxSkeletons; ++i)
            {
                const auto& s = mSkeletonSlots[i];
                if (!s.resident || s.pinned)
                    continue;
                if (s.lastTouch < oldest)
                {
                    oldest   = s.lastTouch;
                    freeSlot = i;
                }
            }
            if (freeSlot == 0xffffffffu)
                return 0xffffffffu;
            EvictSkeletonSlotUnlocked(freeSlot);
        }

        if (!UploadSkeletonSlotUnlocked(freeSlot, key, skeleton, rootJoint))
            return 0xffffffffu;
        if (freeSlot == 0)
            mJointCount = mSkeletonSlots[0].joints;
        return freeSlot;
    }

    void GpuAnimPass::Impl::PinSkeletonSlot(const std::uint32_t slot,
                                            const bool          pinned)
    {
        const SpinLockGuard guard(mSkeletonCacheLock);
        if (slot >= GpuAnimPass::kMaxSkeletons ||
            !mSkeletonSlots[slot].resident)
            return;
        mSkeletonSlots[slot].pinned = pinned;
    }

    std::uint32_t GpuAnimPass::Impl::ResidentSkeletonCount() const
    {
        const SpinLockGuard guard(mSkeletonCacheLock);
        std::uint32_t       n = 0;
        for (const auto& s : mSkeletonSlots)
            if (s.resident)
                ++n;
        return n;
    }

    void GpuAnimPass::Impl::CaptureDebugSnapshot(
        GpuAnimDebugSnapshot& out) const
    {
        const SpinLockGuard clipGuard(mClipCacheLock);
        const SpinLockGuard skelGuard(mSkeletonCacheLock);
        out                         = {};
        out.enabled                 = mEnabled;
        out.quantizedJoints         = mQuantizedJoints;
        out.timestampQueriesEnabled = HasTimestampQueries();
        out.instanceCount           = mInstanceCount;
        out.skeletonJoints          = mJointCount;
        out.maxClips                = GpuAnimPass::kMaxClips;
        out.maxSkeletons            = GpuAnimPass::kMaxSkeletons;
        out.maxBakedJoints          = MaxBakedJoints();
        out.jointsPerClipSlot       = JointsPerClipSlot();
        std::uint32_t resident      = 0;
        for (const auto& s : mClipSlots)
            if (s.resident)
                ++resident;
        out.residentClips          = resident;
        std::uint32_t skelResident = 0;
        for (const auto& s : mSkeletonSlots)
            if (s.resident)
                ++skelResident;
        out.residentSkeletons = skelResident;
        out.extractRequests =
            static_cast<std::uint32_t>(mExtractRequests.size());
        out.slots.resize(GpuAnimPass::kMaxClips);
        for (std::uint32_t i = 0; i < GpuAnimPass::kMaxClips; ++i)
        {
            const auto& s = mClipSlots[i];
            out.slots[i]  = { i,           s.key,    s.resident, s.pinned,
                              s.lastTouch, s.frames, s.joints };
        }
        out.skeletons.resize(GpuAnimPass::kMaxSkeletons);
        for (std::uint32_t i = 0; i < GpuAnimPass::kMaxSkeletons; ++i)
        {
            const auto& s    = mSkeletonSlots[i];
            out.skeletons[i] = { i,           s.key,    s.resident, s.pinned,
                                 s.lastTouch, s.joints, s.rootJoint };
        }
    }

    std::uint32_t GpuAnimPass::Impl::JointsPerClipSlot() const
    {
        return MaxBakedJoints() / GpuAnimPass::kMaxClips;
    }

    std::uint32_t GpuAnimPass::Impl::ResidentClipCount() const
    {
        const SpinLockGuard guard(mClipCacheLock);
        std::uint32_t       n = 0;
        for (const auto& s : mClipSlots)
            if (s.resident)
                ++n;
        return n;
    }

    void GpuAnimPass::Impl::ResetClipCacheUnlocked()
    {
        mClipSlots.fill({});
        mClipTouchClock = 1;
        if (!mClipHeaderBuffer)
            return;
        std::vector<GpuClipHeader> empty(GpuAnimPass::kMaxClips);
        mClipHeaderBuffer->Copy(
            empty.data(),
            static_cast<std::uint32_t>(empty.size() * sizeof(GpuClipHeader)));
    }

    void GpuAnimPass::Impl::ResetClipCache()
    {
        const SpinLockGuard guard(mClipCacheLock);
        assert(!mInstanceStagingOpen &&
               "ResetClipCache during instance staging");
        if (mInstanceStagingOpen)
            return;
        ResetClipCacheUnlocked();
    }

    void GpuAnimPass::Impl::TouchClipSlotUnlocked(const std::uint32_t slot)
    {
        if (slot >= GpuAnimPass::kMaxClips || !mClipSlots[slot].resident)
            return;
        mClipSlots[slot].lastTouch = ++mClipTouchClock;
    }

    void GpuAnimPass::Impl::TouchClipSlot(const std::uint32_t slot)
    {
        const SpinLockGuard guard(mClipCacheLock);
        TouchClipSlotUnlocked(slot);
    }

    void GpuAnimPass::Impl::PinClipSlot(const std::uint32_t slot,
                                        const bool          pinned)
    {
        const SpinLockGuard guard(mClipCacheLock);
        if (slot >= GpuAnimPass::kMaxClips || !mClipSlots[slot].resident)
            return;
        mClipSlots[slot].pinned = pinned;
    }

    std::uint32_t GpuAnimPass::Impl::FindClipSlotUnlocked(
        const std::uint64_t key) const
    {
        if (key == 0)
            return 0xffffffffu;
        for (std::uint32_t i = 0; i < GpuAnimPass::kMaxClips; ++i)
        {
            if (mClipSlots[i].resident && mClipSlots[i].key == key)
                return i;
        }
        return 0xffffffffu;
    }

    std::uint32_t GpuAnimPass::Impl::FindClipSlot(const std::uint64_t key) const
    {
        const SpinLockGuard guard(mClipCacheLock);
        return FindClipSlotUnlocked(key);
    }

    void GpuAnimPass::Impl::EvictClipSlotUnlocked(const std::uint32_t slot)
    {
        if (slot >= GpuAnimPass::kMaxClips)
            return;
        mClipSlots[slot] = {};
        if (!mClipHeaderBuffer)
            return;
        const GpuClipHeader empty {};
        mClipHeaderBuffer->Copy(
            &empty, sizeof(GpuClipHeader),
            static_cast<std::uint64_t>(slot) * sizeof(GpuClipHeader));
    }

    void GpuAnimPass::Impl::EvictClipSlot(const std::uint32_t slot)
    {
        const SpinLockGuard guard(mClipCacheLock);
        assert(!mInstanceStagingOpen &&
               "EvictClipSlot during instance staging");
        if (mInstanceStagingOpen)
            return;
        EvictClipSlotUnlocked(slot);
    }

    bool GpuAnimPass::Impl::UploadClipSlotUnlocked(const std::uint32_t slot,
                                                   const std::uint64_t key,
                                                   const BakedClip&    clip)
    {
        if (slot >= GpuAnimPass::kMaxClips || key == 0 ||
            clip.frameCount == 0 || clip.jointCount == 0 ||
            !mClipHeaderBuffer || !mJointsBuffer)
            return false;

        const auto slab = JointsPerClipSlot();
        const auto need = clip.frameCount * clip.jointCount;
        if (need > slab || need != clip.joints.size())
            return false;

        const auto jointsBase = slot * slab;
        const auto header     = MakeGpuClipHeader(clip, jointsBase);
        mClipHeaderBuffer->Copy(
            &header, sizeof(GpuClipHeader),
            static_cast<std::uint64_t>(slot) * sizeof(GpuClipHeader));

        const auto jointStride =
            mQuantizedJoints ? sizeof(GpuQuantJoint) : sizeof(GpuFloatJoint);
        const auto byteOff =
            static_cast<std::uint64_t>(jointsBase) * jointStride;

        if (mQuantizedJoints)
        {
            const auto packed = PackClipJointsQuant(clip);
            mJointsBuffer->Copy(
                packed.data(),
                static_cast<std::uint64_t>(packed.size()) * jointStride,
                byteOff);
        }
        else
        {
            const auto packed = PackClipJointsFloat(clip);
            mJointsBuffer->Copy(
                packed.data(),
                static_cast<std::uint64_t>(packed.size()) * jointStride,
                byteOff);
        }

        auto&      meta    = mClipSlots[slot];
        const bool keepPin = meta.resident && meta.key == key && meta.pinned;
        meta.key           = key;
        meta.resident      = true;
        meta.pinned        = keepPin;
        meta.frames        = clip.frameCount;
        meta.joints        = clip.jointCount;
        meta.lastTouch     = ++mClipTouchClock;
        return true;
    }

    bool GpuAnimPass::Impl::UploadClipSlot(const std::uint32_t slot,
                                           const std::uint64_t key,
                                           const BakedClip&    clip)
    {
        const SpinLockGuard guard(mClipCacheLock);
        // Direct slot writes stay main-only during packing; workers use
        // EnsureClipResident (free-slot fills under the same lock).
        assert(!mInstanceStagingOpen &&
               "UploadClipSlot during instance staging");
        if (mInstanceStagingOpen)
            return false;
        return UploadClipSlotUnlocked(slot, key, clip);
    }

    std::uint32_t GpuAnimPass::Impl::EnsureClipResident(const std::uint64_t key,
                                                        const BakedClip& clip)
    {
        const SpinLockGuard guard(mClipCacheLock);

        const auto existing = FindClipSlotUnlocked(key);
        if (existing != 0xffffffffu)
        {
            TouchClipSlotUnlocked(existing);
            return existing;
        }

        std::uint32_t freeSlot = 0xffffffffu;
        for (std::uint32_t i = 0; i < GpuAnimPass::kMaxClips; ++i)
        {
            if (!mClipSlots[i].resident)
            {
                freeSlot = i;
                break;
            }
        }

        if (freeSlot == 0xffffffffu)
        {
            // Concurrent packing: never LRU-evict — another worker may already
            // hold a slot index. Main (staging closed) may evict unpinned.
            if (mInstanceStagingOpen)
                return 0xffffffffu;

            std::uint64_t oldest = ~0ull;
            for (std::uint32_t i = 0; i < GpuAnimPass::kMaxClips; ++i)
            {
                const auto& s = mClipSlots[i];
                if (!s.resident || s.pinned)
                    continue;
                if (s.lastTouch < oldest)
                {
                    oldest   = s.lastTouch;
                    freeSlot = i;
                }
            }
            if (freeSlot == 0xffffffffu)
                return 0xffffffffu;
            EvictClipSlotUnlocked(freeSlot);
        }

        if (!UploadClipSlotUnlocked(freeSlot, key, clip))
            return 0xffffffffu;
        return freeSlot;
    }

    void GpuAnimPass::Impl::UploadBakes(const GpuBakePack& pack)
    {
        assert(!mInstanceStagingOpen && "UploadBakes during instance staging");
        if (mInstanceStagingOpen)
            return;
        if (pack.quantized != mQuantizedJoints)
            return;

        const SpinLockGuard guard(mClipCacheLock);
        ResetClipCacheUnlocked();

        // Rebuild contiguous pack into per-slot slabs (slots 0..n pinned).
        // Prefer EnsureClipResident with source BakedClips when streaming;
        // this path keeps a one-shot bulk load for simple demos.
        const auto clipCount =
            std::min(static_cast<std::uint32_t>(pack.headers.size()),
                     GpuAnimPass::kMaxClips);
        if (clipCount == 0)
            return;

        const auto slab = JointsPerClipSlot();
        for (std::uint32_t i = 0; i < clipCount; ++i)
        {
            const auto& srcH = pack.headers[i];
            if (srcH.frameCount == 0 || srcH.jointCount == 0)
                continue;
            const auto need = srcH.frameCount * srcH.jointCount;
            if (need > slab)
                continue;

            GpuClipHeader h = srcH;
            h.jointsBase    = i * slab;
            mClipHeaderBuffer->Copy(
                &h, sizeof(GpuClipHeader),
                static_cast<std::uint64_t>(i) * sizeof(GpuClipHeader));

            const auto jointStride = mQuantizedJoints ? sizeof(GpuQuantJoint)
                                                      : sizeof(GpuFloatJoint);
            const auto dstOff =
                static_cast<std::uint64_t>(h.jointsBase) * jointStride;
            const auto srcBase = srcH.jointsBase;

            if (mQuantizedJoints)
            {
                if (srcBase + need > pack.quantJoints.size())
                    continue;
                mJointsBuffer->Copy(
                    pack.quantJoints.data() + srcBase,
                    static_cast<std::uint64_t>(need) * jointStride,
                    dstOff);
            }
            else
            {
                if (srcBase + need > pack.floatJoints.size())
                    continue;
                mJointsBuffer->Copy(
                    pack.floatJoints.data() + srcBase,
                    static_cast<std::uint64_t>(need) * jointStride,
                    dstOff);
            }

            auto& meta     = mClipSlots[i];
            meta.key       = GpuClipKey(std::to_string(i));
            meta.resident  = true;
            meta.pinned    = true;
            meta.frames    = srcH.frameCount;
            meta.joints    = srcH.jointCount;
            meta.lastTouch = ++mClipTouchClock;
        }
    }

    void GpuAnimPass::Impl::UploadBoneMask(const std::span<const float> weights)
    {
        if (!mBoneMaskBuffer || weights.empty())
            return;
        const auto n = std::min(static_cast<std::uint32_t>(weights.size()),
                                GpuAnimPass::kMaxMaskFloats);
        mBoneMaskBuffer->Copy(weights.data(),
                              static_cast<std::uint32_t>(n * sizeof(float)));
    }

    void GpuAnimPass::Impl::UploadRestJoints(
        const std::span<const GpuFloatJoint> joints)
    {
        UploadRestJoints(0, joints);
    }

    void GpuAnimPass::Impl::UploadRestJoints(
        const std::span<const GpuQuantJoint> joints)
    {
        UploadRestJoints(0, joints);
    }

    void GpuAnimPass::Impl::UploadRestJoints(
        const std::uint32_t skeletonSlot,
        const std::span<const GpuFloatJoint>
            joints)
    {
        if (mQuantizedJoints || !mRestJointsBuffer || joints.empty() ||
            skeletonSlot >= GpuAnimPass::kMaxSkeletons)
            return;
        const auto n = std::min(
            static_cast<std::uint32_t>(joints.size()), GpuAnimPass::kMaxJoints);
        const auto skelBase = skeletonSlot * GpuAnimPass::kMaxJoints;
        mRestJointsBuffer->Copy(
            joints.data(),
            static_cast<std::uint32_t>(n * sizeof(GpuFloatJoint)),
            static_cast<std::uint64_t>(skelBase) * sizeof(GpuFloatJoint));
    }

    void GpuAnimPass::Impl::UploadRestJoints(
        const std::uint32_t skeletonSlot,
        const std::span<const GpuQuantJoint>
            joints)
    {
        if (!mQuantizedJoints || !mRestJointsBuffer || joints.empty() ||
            skeletonSlot >= GpuAnimPass::kMaxSkeletons)
            return;
        const auto n = std::min(
            static_cast<std::uint32_t>(joints.size()), GpuAnimPass::kMaxJoints);
        const auto skelBase = skeletonSlot * GpuAnimPass::kMaxJoints;
        mRestJointsBuffer->Copy(
            joints.data(),
            static_cast<std::uint32_t>(n * sizeof(GpuQuantJoint)),
            static_cast<std::uint64_t>(skelBase) * sizeof(GpuQuantJoint));
    }

    void GpuAnimPass::Impl::BeginInstanceUploads()
    {
        mInstanceStagingOpen = true;
        mInstanceStagingCount.store(0, std::memory_order_relaxed);
    }

    void GpuAnimPass::Impl::ReserveInstanceUploads(const std::uint32_t count)
    {
        SpinLockGuard lock(mInstanceStagingLock);
        if (mInstanceStaging.size() < count)
            mInstanceStaging.resize(count);
    }

    void GpuAnimPass::Impl::UploadInstanceUploads(
        const std::span<const GpuAnimInstance> instances)
    {
        const auto n = static_cast<std::uint32_t>(instances.size());
        if (n == 0)
            return;

        const auto base =
            mInstanceStagingCount.fetch_add(n, std::memory_order_relaxed);

        SpinLockGuard lock(mInstanceStagingLock);
        if (base + n > mInstanceStaging.size())
        {
            const auto grown = std::max(
                base + n,
                std::max<std::uint32_t>(
                    1u,
                    static_cast<std::uint32_t>(mInstanceStaging.size()) * 2u));
            mInstanceStaging.resize(grown);
        }
        std::copy(instances.begin(), instances.end(),
                  mInstanceStaging.begin() + static_cast<std::ptrdiff_t>(base));
    }

    void GpuAnimPass::Impl::EndInstanceUploads()
    {
        mInstanceStagingOpen = false;

        const auto count =
            mInstanceStagingCount.load(std::memory_order_relaxed);
        mInstanceCount = std::min(
            count, static_cast<std::uint32_t>(mInstanceStaging.size()));
        mInstanceCount = std::min(mInstanceCount, GpuAnimPass::kMaxInstances);
        if (mInstanceCount == 0)
            return;

        mInstanceBuffer->Copy(mInstanceStaging.data(),
                              static_cast<std::uint32_t>(
                                  mInstanceCount * sizeof(GpuAnimInstance)));

        // Sticky GPU-owned palette spans for FiF carry (sparse LOD). CPU
        // UploadBoneMatrices unmarks its own span so mixed mode is safe.
        if (mBoneResources)
        {
            for (std::uint32_t i = 0; i < mInstanceCount; ++i)
            {
                const auto& inst = mInstanceStaging[i];
                const auto  jc =
                    std::min(inst.jointCount, GpuAnimPass::kMaxJoints);
                if (jc == 0)
                    continue;
                mBoneResources->MarkGpuOwnedBones(inst.boneOffset, jc);
            }
        }
    }

    void GpuAnimPass::Impl::UploadInstances(
        const std::span<const GpuAnimInstance> instances)
    {
        BeginInstanceUploads();
        ReserveInstanceUploads(static_cast<std::uint32_t>(instances.size()));
        UploadInstanceUploads(instances);
        EndInstanceUploads();
    }

    void GpuAnimPass::Impl::Dispatch(const skr::Arc<CommandPool>& commandPool,
                                     const std::uint32_t frameIndex) const
    {
        if (!mEnabled || mInstanceCount == 0 || !mBoneResources || !mPipeline)
            return;

        auto       commandBuffer = commandPool->GetCommandBuffer();
        const auto slot          = extractSlot(frameIndex);
        const auto qBase         = timestampQueryBase(frameIndex);

        if (mTimestampPool)
        {
            commandBuffer.resetQueryPool(
                mTimestampPool, qBase, GpuAnimPass::kTimestampQueriesPerSlot);
            mTimingPending[slot]       = 1;
            mTimingHasCarry[slot]      = mCopyPrevBones ? 1 : 0;
            mTimingInstanceCount[slot] = mInstanceCount;
            mTimingSourceFrame[slot]   = frameIndex;
        }

        if (mCopyPrevBones)
        {
            writeTimestamp(commandBuffer, qBase + 0,
                           vk::PipelineStageFlagBits::eTopOfPipe);
            // Sparse LOD: seed GPU-owned spans from the previous FiF slot so
            // instances omitted this frame keep a continuous pose. CPU Upload
            // spans stay untouched (see BoneMatrixResources::mGpuOwned).
            mBoneResources->RecordCarryBonesFromPreviousFrame(commandBuffer,
                                                              frameIndex);
            mBoneResources->RecordCopyCurrentToPrev(commandBuffer, frameIndex);
            writeTimestamp(commandBuffer, qBase + 1,
                           vk::PipelineStageFlagBits::eTransfer);
        }
        else if (mTimestampPool)
        {
            // Keep query indices stable when carry is disabled.
            writeTimestamp(commandBuffer, qBase + 0,
                           vk::PipelineStageFlagBits::eTopOfPipe);
            writeTimestamp(commandBuffer, qBase + 1,
                           vk::PipelineStageFlagBits::eTopOfPipe);
        }

        writeTimestamp(commandBuffer, qBase + 2,
                       vk::PipelineStageFlagBits::eComputeShader);
        recordCompute(commandBuffer, frameIndex);
        writeTimestamp(commandBuffer, qBase + 3,
                       vk::PipelineStageFlagBits::eComputeShader);
        recordJointExtract(commandBuffer, frameIndex);
    }

    void GpuAnimPass::Impl::DispatchImmediate(
        const skr::Arc<CommandPool>& commandPool,
        const std::uint32_t          frameIndex) const
    {
        if (!mEnabled || mInstanceCount == 0 || !mBoneResources || !mPipeline ||
            !mDevice)
            return;

        mDevice->Get().waitIdle();

        auto cb = commandPool->CreateCommandBuffer();
        cb.begin(vk::CommandBufferBeginInfo().setFlags(
            vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
        recordCompute(cb, frameIndex);
        recordJointExtract(cb, frameIndex);
        cb.end();

        const auto submitInfo =
            vk::SubmitInfo().setCommandBufferCount(1).setPCommandBuffers(&cb);
        mDevice->GetGraphicsQueue().submit(submitInfo);
        mDevice->GetGraphicsQueue().waitIdle();
        commandPool->FreeCommandBuffer(cb);
    }

    void GpuAnimPass::Impl::recordCompute(const vk::CommandBuffer commandBuffer,
                                          const std::uint32_t frameIndex) const
    {
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, mPipeline);
        const auto boneSet = mBoneResources->GetSet(frameIndex);
        const auto sets    = std::array { boneSet, mAnimSet };
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute, mPipelineLayout, 0, sets, {});

        PushConstants pc {};
        pc.instanceCount = mInstanceCount;
        pc.jointCount    = mJointCount;
        pc.rootJoint     = mRootJoint;
        commandBuffer.pushConstants(
            mPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0,
            sizeof(PushConstants), &pc);

        const auto groups = (mInstanceCount + 63u) / 64u;
        commandBuffer.dispatch(groups, 1, 1);

        const auto bonesOff = mBoneResources->BonesByteOffset(frameIndex);
        const auto bytes    = mBoneResources->PaletteBytes();
        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eVertexShader, {}, {},
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setBuffer(mBoneResources->GetBuffer()->Get())
                .setOffset(bonesOff)
                .setSize(bytes),
            {});
    }

    void GpuAnimPass::Impl::recordJointExtract(
        const vk::CommandBuffer commandBuffer,
        const std::uint32_t     frameIndex) const
    {
        if (!mExtractRingBuffer || !mBoneResources || mExtractRequests.empty())
            return;

        const auto slot = extractSlot(frameIndex);
        const auto n =
            std::min(static_cast<std::uint32_t>(mExtractRequests.size()),
                     GpuAnimPass::kMaxExtractJoints);
        if (n == 0)
            return;

        mExtractMeta[slot].assign(mExtractRequests.begin(),
                                  mExtractRequests.begin() + n);
        mExtractCounts[slot]      = n;
        mExtractSourceFrame[slot] = frameIndex;
        // Valid for Poll only after GPU finishes this FiF slot (next wrap).
        mExtractValid[slot] = 1;

        const auto bonesBase = mBoneResources->BonesByteOffset(frameIndex);
        const auto dstBase =
            static_cast<vk::DeviceSize>(slot) * extractSlotBytes();
        const auto matBytes = static_cast<vk::DeviceSize>(sizeof(glm::mat4));

        // Compute → transfer; vertex still uses bones via prior barrier.
        std::vector<vk::BufferCopy> regions;
        regions.reserve(n);
        for (std::uint32_t i = 0; i < n; ++i)
        {
            const auto& req = mExtractRequests[i];
            const auto  src =
                bonesBase +
                static_cast<vk::DeviceSize>(req.boneOffset + req.jointIndex) *
                    matBytes;
            regions.push_back(vk::BufferCopy()
                                  .setSrcOffset(src)
                                  .setDstOffset(dstBase + i * matBytes)
                                  .setSize(matBytes));
        }

        const auto rangeBytes = n * matBytes;
        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader |
                vk::PipelineStageFlagBits::eVertexShader,
            vk::PipelineStageFlagBits::eTransfer, {}, {},
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eShaderWrite |
                                  vk::AccessFlagBits::eShaderRead)
                .setDstAccessMask(vk::AccessFlagBits::eTransferRead)
                .setBuffer(mBoneResources->GetBuffer()->Get())
                .setOffset(bonesBase)
                .setSize(mBoneResources->PaletteBytes()),
            {});
        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eHost,
            vk::PipelineStageFlagBits::eTransfer, {}, {},
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eHostRead)
                .setDstAccessMask(vk::AccessFlagBits::eTransferWrite)
                .setBuffer(mExtractRingBuffer->Get())
                .setOffset(dstBase)
                .setSize(rangeBytes),
            {});

        commandBuffer.copyBuffer(mBoneResources->GetBuffer()->Get(),
                                 mExtractRingBuffer->Get(), regions);

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eHost, {}, {},
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                .setDstAccessMask(vk::AccessFlagBits::eHostRead)
                .setBuffer(mExtractRingBuffer->Get())
                .setOffset(dstBase)
                .setSize(rangeBytes),
            {});
    }

    bool GpuAnimPass::Impl::ReadbackBones(
        const skr::Arc<CommandPool>& commandPool,
        const std::uint32_t          frameIndex,
        const std::uint32_t          boneOffset,
        const std::uint32_t          count,
        const std::span<glm::mat4>
            out) const
    {
        if (!mBoneResources || !mReadbackBuffer || count == 0 ||
            out.size() < count || mJointCount == 0)
            return false;

        const auto n = std::min({ count, mJointCount, GpuAnimPass::kMaxJoints,
                                  static_cast<std::uint32_t>(out.size()) });
        const auto byteCount =
            static_cast<vk::DeviceSize>(n) * sizeof(glm::mat4);
        const auto srcOff =
            mBoneResources->BonesByteOffset(frameIndex) +
            static_cast<vk::DeviceSize>(boneOffset) * sizeof(glm::mat4);

        mDevice->Get().waitIdle();

        auto cb = commandPool->CreateCommandBuffer();
        cb.begin(vk::CommandBufferBeginInfo().setFlags(
            vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
        cb.pipelineBarrier(
            vk::PipelineStageFlagBits::eVertexShader |
                vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eTransfer, {}, {},
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eShaderWrite |
                                  vk::AccessFlagBits::eShaderRead)
                .setDstAccessMask(vk::AccessFlagBits::eTransferRead)
                .setBuffer(mBoneResources->GetBuffer()->Get())
                .setOffset(srcOff)
                .setSize(byteCount),
            {});
        const auto region =
            vk::BufferCopy().setSrcOffset(srcOff).setDstOffset(0).setSize(
                byteCount);
        cb.copyBuffer(mBoneResources->GetBuffer()->Get(),
                      mReadbackBuffer->Get(), region);
        cb.end();

        const auto submitInfo =
            vk::SubmitInfo().setCommandBufferCount(1).setPCommandBuffers(&cb);
        mDevice->GetGraphicsQueue().submit(submitInfo);
        mDevice->GetGraphicsQueue().waitIdle();
        commandPool->FreeCommandBuffer(cb);

        const void* mapped = mReadbackBuffer->GetMapped();
        if (!mapped)
            return false;
        std::memcpy(out.data(), mapped, static_cast<std::size_t>(byteCount));
        return true;
    }

} // namespace FREYA_NAMESPACE
