#pragma once

#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/Device.hpp"

#include <cstdint>
#include <span>
#include <utility>
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
     * Carry / copy-to-prev only touch **GPU-owned** bone ranges (see
     * MarkGpuOwnedBones). CPU Upload unmarks its span so a mixed frame can
     * UploadBoneMatrices and SetCopyPrevBones(true) without the FiF carry
     * wiping hero/NPC skins. UploadInstances should run after CPU upload so
     * wild slots are re-marked the same frame.
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
         * Writes `bones` at `boneOffset` (count clipped to capacity). Unused
         * slots beyond the span are left untouched on the device. Unmarks the
         * written span from GPU-owned carry ranges.
         */
        void Upload(std::uint32_t frameIndex, std::span<const glm::mat4> bones,
                    std::uint32_t boneOffset = 0);

        /**
         * @brief Sticky range written by GpuAnimPass instances (FiF carry).
         *
         * Merged across frames so sparse LOD can omit an instance while still
         * carrying its last pose. CPU Upload clears its own span.
         */
        void MarkGpuOwnedBones(std::uint32_t boneOffset, std::uint32_t count);

        void UnmarkGpuOwnedBones(std::uint32_t boneOffset, std::uint32_t count);

        void ClearGpuOwnedBones();

        /**
         * @brief GPU: copy bones[(fi-1)%N] → bones[fi] for GPU-owned ranges.
         *
         * No-op when frameCount < 2 or no GPU-owned bones. Call before
         * RecordCopyCurrentToPrev when the compute pass may update only a
         * subset of instances.
         */
        void RecordCarryBonesFromPreviousFrame(vk::CommandBuffer commandBuffer,
                                               std::uint32_t frameIndex) const;

        /**
         * @brief GPU: copy bones → prevBones for GPU-owned ranges (TAA).
         *
         * Call before a compute pass that overwrites bones[]. CPU spans keep
         * the prevBones written by Upload. Inserts transfer barriers around
         * the copies.
         */
        void RecordCopyCurrentToPrev(vk::CommandBuffer commandBuffer,
                                     std::uint32_t     frameIndex) const;

      private:
        [[nodiscard]] vk::DeviceSize frameBytes() const
        {
            return static_cast<vk::DeviceSize>(mCapacity) * 2u *
                   sizeof(glm::mat4);
        }

        using Interval = std::pair<std::uint32_t, std::uint32_t>;

        void addOwnedInterval(std::uint32_t begin, std::uint32_t end);
        void removeOwnedInterval(std::uint32_t begin, std::uint32_t end);

        void recordOwnedCopies(vk::CommandBuffer      commandBuffer,
                               vk::DeviceSize         srcBase,
                               vk::DeviceSize         dstBase,
                               vk::DeviceSize         barrierOffset,
                               vk::DeviceSize         barrierSize,
                               vk::PipelineStageFlags dstStages,
                               vk::AccessFlags        dstAccess) const;

        skr::Arc<Device>               mDevice;
        std::uint32_t                  mFrameCount = 1;
        std::uint32_t                  mCapacity   = kDefaultCapacity;
        skr::Arc<Buffer>               mBuffer;
        vk::DescriptorSetLayout        mLayout;
        vk::DescriptorPool             mPool;
        std::vector<vk::DescriptorSet> mSets;
        std::vector<glm::mat4>         mCpuPrev;
        bool                           mHasUploaded = false;
        /// Sorted non-overlapping [begin, end) bone indices owned by GPU anim.
        std::vector<Interval> mGpuOwned;
    };

} // namespace FREYA_NAMESPACE
