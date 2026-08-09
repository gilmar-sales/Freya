#pragma once

#include "Freya/Asset/BoneMatrixResources.hpp"
#include "Freya/Asset/GpuAnimation.hpp"
#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"

#include <cstdint>
#include <span>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Compute path that fills BoneMatrixResources bones[] (GPU anim).
     */
    class GpuAnimPass
    {
      public:
        static constexpr std::uint32_t kMaxJoints      = 128;
        static constexpr std::uint32_t kMaxInstances   = 2048;
        static constexpr std::uint32_t kMaxClips       = 8;
        static constexpr std::uint32_t kMaxBakedJoints = 65536;
        static constexpr std::uint32_t kMaxMaskFloats  = kMaxJoints;

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
                    const skr::Arc<Buffer>&              readbackBuffer);

        ~GpuAnimPass();

        GpuAnimPass(const GpuAnimPass&)            = delete;
        GpuAnimPass& operator=(const GpuAnimPass&) = delete;

        void SetEnabled(const bool enabled) { mEnabled = enabled; }
        [[nodiscard]] bool IsEnabled() const { return mEnabled; }

        /**
         * @brief When true, Dispatch carries bones from the previous FiF slot,
         * then copies bones→prevBones, then compute (sparse-safe LOD).
         * Set false if the CPU upload path already filled this frame's
         * palettes.
         */
        void SetCopyPrevBones(const bool enabled) { mCopyPrevBones = enabled; }

        void UploadSkeleton(const GpuSkeletonPack& skeleton);
        void UploadBakes(const GpuBakePack& pack);
        void UploadBoneMask(std::span<const float> weights);
        void UploadRestJoints(std::span<const GpuBakedJoint> joints);
        void UploadInstances(std::span<const GpuAnimInstance> instances);

        [[nodiscard]] std::uint32_t GetInstanceCount() const
        {
            return mInstanceCount;
        }

        void Dispatch(const skr::Arc<CommandPool>& commandPool,
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
        struct PushConstants
        {
            std::uint32_t instanceCount = 0;
            std::uint32_t jointCount    = 0;
            std::uint32_t _pad0         = 0;
            std::uint32_t _pad1         = 0;
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
        skr::Arc<Buffer> mReadbackBuffer;

        bool          mEnabled       = false;
        bool          mCopyPrevBones = true;
        std::uint32_t mInstanceCount = 0;
        std::uint32_t mJointCount    = 0;
    };

} // namespace FREYA_NAMESPACE
