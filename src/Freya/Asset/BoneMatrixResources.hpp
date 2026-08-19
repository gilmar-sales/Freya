#pragma once

#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/Device.hpp"

#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Per-frame bone palette SSBO (current + previous for TAA).
     *
     * Descriptor layout: binding 0 = bones[], binding 1 = prevBones[].
     * Upload (CPU path) copies the previous CPU palette into prevBones, then
     * writes the new palette into bones (identity fill for unused slots).
     *
     * GPU anim path (full or sparse): RecordCarryBonesFromPreviousFrame →
     * RecordCopyCurrentToPrev → compute write into bones[], then a
     * shader-read barrier before vertex skinning.
     *
     * Carry is required for sparse instance updates with FiF>1: otherwise
     * non-dispatched foxes keep an old pose from this ring slot and flicker
     * between time-shifted poses each frame.
     */
    class BoneMatrixResources
    {
      public:
        static constexpr std::uint32_t kDefaultCapacity = 32768;

        BoneMatrixResources(const skr::Arc<Device>& device,
                            std::uint32_t           frameCount,
                            std::uint32_t capacity = kDefaultCapacity);

        ~BoneMatrixResources();

        BoneMatrixResources(const BoneMatrixResources&)            = delete;
        BoneMatrixResources& operator=(const BoneMatrixResources&) = delete;

        [[nodiscard]] vk::DescriptorSetLayout GetLayout() const
        {
            return mLayout;
        }

        [[nodiscard]] vk::DescriptorSet GetSet(std::uint32_t frameIndex) const
        {
            return mSets[frameIndex % mFrameCount];
        }

        [[nodiscard]] std::uint32_t GetCapacity() const { return mCapacity; }

        [[nodiscard]] std::uint32_t GetFrameCount() const
        {
            return mFrameCount;
        }

        [[nodiscard]] const skr::Arc<Buffer>& GetBuffer() const
        {
            return mBuffer;
        }

        [[nodiscard]] vk::DeviceSize PaletteBytes() const
        {
            return static_cast<vk::DeviceSize>(mCapacity) * sizeof(glm::mat4);
        }

        [[nodiscard]] vk::DeviceSize BonesByteOffset(
            std::uint32_t frameIndex) const
        {
            const auto fi = frameIndex % mFrameCount;
            return static_cast<vk::DeviceSize>(fi) * frameBytes();
        }

        [[nodiscard]] vk::DeviceSize PrevBonesByteOffset(
            std::uint32_t frameIndex) const
        {
            return BonesByteOffset(frameIndex) + PaletteBytes();
        }

        /**
         * @brief Upload skin matrices for this in-flight frame slot.
         *
         * `bones.size()` must be ≤ capacity. Unused slots stay identity.
         */
        void Upload(std::uint32_t frameIndex, std::span<const glm::mat4> bones);

        /**
         * @brief GPU: copy bones[(fi-1)%N] → bones[fi] (pose continuity).
         *
         * No-op when frameCount < 2. Call before RecordCopyCurrentToPrev when
         * the compute pass may update only a subset of instances.
         */
        void RecordCarryBonesFromPreviousFrame(vk::CommandBuffer commandBuffer,
                                               std::uint32_t frameIndex) const;

        /**
         * @brief GPU: copy bones → prevBones for `frameIndex` (TAA velocity).
         *
         * Call before a compute pass that overwrites bones[]. Inserts
         * transfer barriers around the copy.
         */
        void RecordCopyCurrentToPrev(vk::CommandBuffer commandBuffer,
                                     std::uint32_t     frameIndex) const;

      private:
        [[nodiscard]] vk::DeviceSize frameBytes() const
        {
            return static_cast<vk::DeviceSize>(mCapacity) * 2u *
                   sizeof(glm::mat4);
        }

        skr::Arc<Device>               mDevice;
        std::uint32_t                  mFrameCount = 1;
        std::uint32_t                  mCapacity   = kDefaultCapacity;
        skr::Arc<Buffer>               mBuffer;
        vk::DescriptorSetLayout        mLayout;
        vk::DescriptorPool             mPool;
        std::vector<vk::DescriptorSet> mSets;
        std::vector<glm::mat4>         mCpuPrev;
        bool                           mHasUploaded = false;
    };

} // namespace FREYA_NAMESPACE
