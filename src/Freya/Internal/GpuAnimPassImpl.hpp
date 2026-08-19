#pragma once

#include "Freya/Core/GpuAnimPass.hpp"

#include "Freya/Asset/BoneMatrixResources.hpp"
#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <vulkan/vulkan.hpp>

namespace FREYA_NAMESPACE
{
    class GpuAnimPass::Impl
    {
      public:
        Impl(const skr::Arc<Device>&              device,
             const skr::Arc<BoneMatrixResources>& boneResources,
             vk::PipelineLayout                   pipelineLayout,
             vk::Pipeline                         pipeline,
             vk::DescriptorSetLayout              animSetLayout,
             vk::DescriptorPool                   animPool,
             vk::DescriptorSet                    animSet,
             const skr::Arc<Buffer>&              parentsBuffer,
             const skr::Arc<Buffer>&              invBindBuffer,
             const skr::Arc<Buffer>&              clipHeaderBuffer,
             const skr::Arc<Buffer>&              jointsBuffer,
             const skr::Arc<Buffer>&              instanceBuffer,
             const skr::Arc<Buffer>&              boneMaskBuffer,
             const skr::Arc<Buffer>&              restJointsBuffer,
             const skr::Arc<Buffer>&              localScratchBuffer,
             const skr::Arc<Buffer>&              globalScratchBuffer,
             const skr::Arc<Buffer>&              readbackBuffer,
             const skr::Arc<Buffer>&              extractRingBuffer,
             std::uint32_t                        frameCount,
             bool                                 quantizedJoints);

        ~Impl();

        void SetEnabled(const bool enabled) { mEnabled = enabled; }
        [[nodiscard]] bool IsEnabled() const { return mEnabled; }
        [[nodiscard]] bool UsesQuantizedJoints() const
        {
            return mQuantizedJoints;
        }
        [[nodiscard]] std::uint32_t MaxBakedJoints() const
        {
            return mQuantizedJoints ? GpuAnimPass::kMaxBakedJointsQuant
                                    : GpuAnimPass::kMaxBakedJointsFloat;
        }

        void SetCopyPrevBones(const bool enabled) { mCopyPrevBones = enabled; }

        void SetRigIndices(
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
        }

        void SetLookClamp(const float maxYawRad, const float maxPitchRad)
        {
            mLookMaxYawRad   = maxYawRad;
            mLookMaxPitchRad = maxPitchRad;
        }

        [[nodiscard]] std::uint32_t GetDefaultLookJoint() const
        {
            return mLookJoint;
        }
        [[nodiscard]] std::uint32_t GetDefaultIkRoot() const { return mIkRoot; }
        [[nodiscard]] std::uint32_t GetDefaultIkMid() const { return mIkMid; }
        [[nodiscard]] std::uint32_t GetDefaultIkTip() const { return mIkTip; }
        [[nodiscard]] glm::vec3     GetDefaultLookLocalForward() const
        {
            return mLookLocalForward;
        }
        [[nodiscard]] float GetDefaultLookMaxYaw() const
        {
            return mLookMaxYawRad;
        }
        [[nodiscard]] float GetDefaultLookMaxPitch() const
        {
            return mLookMaxPitchRad;
        }

        void SetJointExtractList(std::span<const GpuJointExtractRequest> reqs);
        bool PollJointExtract(std::uint32_t frameIndex,
                              std::span<GpuJointExtractSample>
                                             out,
                              std::uint32_t* outCount) const;
        bool PollTiming(std::uint32_t        frameIndex,
                        GpuAnimTimingSample& out) const;
        [[nodiscard]] bool HasTimestampQueries() const
        {
            return static_cast<bool>(mTimestampPool);
        }

        void UploadSkeleton(const GpuSkeletonPack& skeleton);
        void UploadBakes(const GpuBakePack& pack);
        void ResetClipCache();
        bool UploadClipSlot(std::uint32_t slot, std::uint64_t key,
                            const BakedClip& clip);
        [[nodiscard]] std::uint32_t EnsureClipResident(std::uint64_t    key,
                                                       const BakedClip& clip);
        void PinClipSlot(std::uint32_t slot, bool pinned);
        void TouchClipSlot(std::uint32_t slot);
        void EvictClipSlot(std::uint32_t slot);
        [[nodiscard]] std::uint32_t FindClipSlot(std::uint64_t key) const;
        [[nodiscard]] std::uint32_t JointsPerClipSlot() const;
        [[nodiscard]] std::uint32_t ResidentClipCount() const;
        void CaptureDebugSnapshot(GpuAnimDebugSnapshot& out) const;
        void UploadBoneMask(std::span<const float> weights);
        void UploadRestJoints(std::span<const GpuFloatJoint> joints);
        void UploadRestJoints(std::span<const GpuQuantJoint> joints);
        void UploadInstances(std::span<const GpuAnimInstance> instances);
        [[nodiscard]] std::uint32_t GetInstanceCount() const
        {
            return mInstanceCount;
        }

        void Dispatch(const skr::Arc<CommandPool>& commandPool,
                      std::uint32_t                frameIndex) const;
        void DispatchImmediate(const skr::Arc<CommandPool>& commandPool,
                               std::uint32_t                frameIndex) const;
        bool ReadbackBones(const skr::Arc<CommandPool>& commandPool,
                           std::uint32_t                frameIndex,
                           std::uint32_t                boneOffset,
                           std::uint32_t                count,
                           std::span<glm::mat4>
                               out) const;

        void recordCompute(vk::CommandBuffer commandBuffer,
                           std::uint32_t     frameIndex) const;
        void recordJointExtract(vk::CommandBuffer commandBuffer,
                                std::uint32_t     frameIndex) const;
        void createTimestampPool();
        void writeTimestamp(vk::CommandBuffer         commandBuffer,
                            std::uint32_t             queryIndex,
                            vk::PipelineStageFlagBits stage) const;

        [[nodiscard]] std::uint32_t extractSlot(std::uint32_t frameIndex) const
        {
            return mFrameCount == 0 ? 0u : (frameIndex % mFrameCount);
        }

        [[nodiscard]] std::uint32_t timestampQueryBase(
            std::uint32_t frameIndex) const
        {
            return extractSlot(frameIndex) *
                   GpuAnimPass::kTimestampQueriesPerSlot;
        }

        [[nodiscard]] vk::DeviceSize extractSlotBytes() const
        {
            return static_cast<vk::DeviceSize>(GpuAnimPass::kMaxExtractJoints) *
                   sizeof(glm::mat4);
        }

        struct PushConstants
        {
            std::uint32_t instanceCount = 0;
            std::uint32_t jointCount    = 0;
            std::uint32_t rootJoint     = 0xffffffffu;
            std::uint32_t _pad0         = 0;
        };

        skr::Arc<Device>              mDevice;
        skr::Arc<BoneMatrixResources> mBoneResources;
        vk::PipelineLayout            mPipelineLayout;
        vk::Pipeline                  mPipeline;
        vk::DescriptorSetLayout       mAnimSetLayout;
        vk::DescriptorPool            mAnimPool;
        vk::DescriptorSet             mAnimSet;

        skr::Arc<Buffer> mParentsBuffer;
        skr::Arc<Buffer> mInvBindBuffer;
        skr::Arc<Buffer> mClipHeaderBuffer;
        skr::Arc<Buffer> mJointsBuffer;
        skr::Arc<Buffer> mInstanceBuffer;
        skr::Arc<Buffer> mBoneMaskBuffer;
        skr::Arc<Buffer> mRestJointsBuffer;
        skr::Arc<Buffer> mLocalScratchBuffer;
        skr::Arc<Buffer> mGlobalScratchBuffer;
        skr::Arc<Buffer> mReadbackBuffer;
        skr::Arc<Buffer> mExtractRingBuffer;

        bool          mEnabled          = false;
        bool          mCopyPrevBones    = true;
        bool          mQuantizedJoints  = true;
        std::uint32_t mInstanceCount    = 0;
        std::uint32_t mJointCount       = 0;
        std::uint32_t mFrameCount       = 1;
        std::uint32_t mLookJoint        = 0xffffffffu;
        std::uint32_t mIkRoot           = 0;
        std::uint32_t mIkMid            = 0;
        std::uint32_t mIkTip            = 0;
        std::uint32_t mRootJoint        = 0xffffffffu;
        glm::vec3     mLookLocalForward = { 0.f, 0.f, 1.f };
        float         mLookMaxYawRad    = 1.2f;
        float         mLookMaxPitchRad  = 0.8f;

        struct ClipSlotMeta
        {
            std::uint64_t key       = 0;
            bool          resident  = false;
            bool          pinned    = false;
            std::uint64_t lastTouch = 0;
            std::uint32_t frames    = 0;
            std::uint32_t joints    = 0;
        };

        std::array<ClipSlotMeta, GpuAnimPass::kMaxClips> mClipSlots {};
        std::uint64_t                                    mClipTouchClock = 1;

        std::vector<GpuJointExtractRequest> mExtractRequests;
        mutable std::vector<std::vector<GpuJointExtractRequest>> mExtractMeta;
        mutable std::vector<std::uint32_t>                       mExtractCounts;
        mutable std::vector<std::uint8_t>                        mExtractValid;
        mutable std::vector<std::uint32_t> mExtractSourceFrame;

        vk::QueryPool                      mTimestampPool     = nullptr;
        float                              mTimestampPeriodNs = 0.f;
        mutable std::vector<std::uint8_t>  mTimingPending;
        mutable std::vector<std::uint8_t>  mTimingHasCarry;
        mutable std::vector<std::uint32_t> mTimingInstanceCount;
        mutable std::vector<std::uint32_t> mTimingSourceFrame;
    };
} // namespace FREYA_NAMESPACE
