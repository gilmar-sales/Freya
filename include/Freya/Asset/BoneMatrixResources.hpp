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
     * Upload copies the previous CPU palette into prevBones, then writes the
     * new palette into bones (identity fill for unused slots).
     */
    class BoneMatrixResources
    {
      public:
        static constexpr std::uint32_t kDefaultCapacity = 32768;

        BoneMatrixResources(const skr::Arc<Device>& device,
                            std::uint32_t           frameCount,
                            std::uint32_t           capacity = kDefaultCapacity);

        ~BoneMatrixResources();

        BoneMatrixResources(const BoneMatrixResources&) = delete;
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

        /**
         * @brief Upload skin matrices for this in-flight frame slot.
         *
         * `bones.size()` must be ≤ capacity. Unused slots stay identity.
         */
        void Upload(std::uint32_t frameIndex, std::span<const glm::mat4> bones);

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
