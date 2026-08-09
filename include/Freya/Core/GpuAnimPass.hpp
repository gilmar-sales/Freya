#pragma once

#include "Freya/Asset/BoneMatrixResources.hpp"
#include "Freya/Asset/GpuAnimDebug.hpp"
#include "Freya/Asset/GpuAnimation.hpp"
#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Compute path that fills BoneMatrixResources bones[] (GPU anim).
     *
     * Optional joint extract: after bake, copies a tiny list of skin matrices
     * into a host-visible FiF ring. Call PollJointExtract at the start of the
     * next CPU Update (before the next Dispatch overwrites that slot) for
     * 1-frame-late gameplay (footsteps, VFX attach, etc.). Same-frame pose
     * still needs a CPU path or a sync readback.
     */
    class GpuAnimPass
    {
      public:
        static constexpr std::uint32_t kMaxJoints    = 128;
        static constexpr std::uint32_t kMaxInstances = 2048;
        static constexpr std::uint32_t kMaxClips     = 24;
        /// Float storage capacity (48 B/joint).
        static constexpr std::uint32_t kMaxBakedJointsFloat = 65536;
        /// Quantized capacity: same VRAM as float pool (65536×48 / 16).
        static constexpr std::uint32_t kMaxBakedJointsQuant = 196608;
        static constexpr std::uint32_t kMaxMaskFloats       = kMaxJoints;
        static constexpr std::uint32_t kMaxExtractJoints    = 64;
        /// carryBegin, carryEnd, bakeBegin, bakeEnd per FiF slot.
        static constexpr std::uint32_t kTimestampQueriesPerSlot = 4;

        GpuAnimPass(const skr::Arc<Device>&              device,
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

        ~GpuAnimPass();

        GpuAnimPass(const GpuAnimPass&)            = delete;
        GpuAnimPass& operator=(const GpuAnimPass&) = delete;

        void SetEnabled(const bool enabled) { mEnabled = enabled; }
        [[nodiscard]] bool IsEnabled() const { return mEnabled; }

        [[nodiscard]] bool UsesQuantizedJoints() const
        {
            return mQuantizedJoints;
        }

        [[nodiscard]] std::uint32_t MaxBakedJoints() const
        {
            return mQuantizedJoints ? kMaxBakedJointsQuant
                                    : kMaxBakedJointsFloat;
        }

        /**
         * @brief When true, Dispatch carries bones from the previous FiF slot,
         * then copies bones→prevBones, then compute (sparse-safe LOD).
         * Set false if the CPU upload path already filled this frame's
         * palettes.
         */
        void SetCopyPrevBones(const bool enabled) { mCopyPrevBones = enabled; }

        /**
         * @brief Shared skeleton root + optional defaults for instance packing.
         *
         * Look / IK joint indices and aim axes are **per instance**
         * (`GpuAnimInstance`). Defaults stored here can be copied when filling
         * instances. Only `rootJoint` is pushed each Dispatch (CancelRootXZ).
         */
        void SetRigIndices(const std::uint32_t lookJoint,
                           const std::uint32_t ikRoot,
                           const std::uint32_t ikMid, const std::uint32_t ikTip,
                           const std::uint32_t rootJoint,
                           const glm::vec3 lookLocalForward = { 0.f, 0.f, 1.f },
                           const float     lookMaxYawRad    = 1.2f,
                           const float     lookMaxPitchRad  = 0.8f)
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

        /** Default look clamp for instance packing (golden may override). */
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

        /**
         * @brief Configure which palette entries to copy each Dispatch.
         *
         * Empty clears extract. Cap is #kMaxExtractJoints. Changes apply on
         * the next Dispatch (not retroactive to in-flight rings).
         */
        void SetJointExtractList(std::span<const GpuJointExtractRequest> reqs);

        /**
         * @brief Read extracts recorded for the previous CPU frame.
         *
         * Call once at Update start with the current FiF index (after
         * BeginFrame / before that slot's next extract overwrite). Returns
         * false until at least one Dispatch has filled the previous slot.
         */
        bool PollJointExtract(std::uint32_t frameIndex,
                              std::span<GpuJointExtractSample>
                                             out,
                              std::uint32_t* outCount = nullptr) const;

        /**
         * @brief Read carry/bake GPU timestamps for the previous FiF slot.
         *
         * Call at Update start (same cadence as #PollJointExtract). Returns
         * false when queries are unavailable or results are not ready yet.
         */
        bool PollTiming(std::uint32_t        frameIndex,
                        GpuAnimTimingSample& out) const;

        [[nodiscard]] bool HasTimestampQueries() const
        {
            return static_cast<bool>(mTimestampPool);
        }

        void UploadSkeleton(const GpuSkeletonPack& skeleton);
        /**
         * @brief Bulk replace: reset clip slots, upload pack into slots
         * 0..n-1 (pinned). Prefer #EnsureClipResident / #UploadClipSlot for
         * streaming.
         */
        void UploadBakes(const GpuBakePack& pack);
        /**
         * @brief Clear all clip slots and zero headers on GPU.
         */
        void ResetClipCache();
        /**
         * @brief Write one baked clip into a fixed slot (slab allocator).
         * @return false if slot OOB, empty clip, or joints exceed slab.
         */
        bool UploadClipSlot(std::uint32_t slot, std::uint64_t key,
                            const BakedClip& clip);
        /**
         * @brief Find or upload clip; may evict unpinned LRU when full.
         * @return slot index, or 0xffffffff on failure.
         */
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

        /**
         * @brief One-shot submit + waitIdle (golden / debug compares).
         * Does not carry/copy prev bones.
         */
        void DispatchImmediate(const skr::Arc<CommandPool>& commandPool,
                               std::uint32_t                frameIndex) const;

        /**
         * @brief Device idle + copy bones slice to readback → `out`.
         */
        bool ReadbackBones(const skr::Arc<CommandPool>& commandPool,
                           std::uint32_t                frameIndex,
                           std::uint32_t                boneOffset,
                           std::uint32_t                count,
                           std::span<glm::mat4>
                               out) const;

      private:
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
            return extractSlot(frameIndex) * kTimestampQueriesPerSlot;
        }

        [[nodiscard]] vk::DeviceSize extractSlotBytes() const
        {
            return static_cast<vk::DeviceSize>(kMaxExtractJoints) *
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

        std::array<ClipSlotMeta, kMaxClips> mClipSlots {};
        std::uint64_t                       mClipTouchClock = 1;

        std::vector<GpuJointExtractRequest> mExtractRequests;
        /// Per FiF slot: request snapshot + count + validity for Poll.
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
