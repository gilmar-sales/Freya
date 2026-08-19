#pragma once

#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"

#include <vector>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Hierarchical Z pyramid built from the scene depth buffer.
     *
     * Used for temporal occlusion culling (previous-frame Hi-Z). Reduce
     * stores the farthest depth per texel (min under reverse-Z).
     *
     * Descriptor sets are allocated once per frame-in-flight so Build can
     * update them safely after that frame's fence has retired.
     */
    class HiZPyramid
    {
      public:
        static constexpr std::uint32_t kMaxMipLevels = 16;

        HiZPyramid(const skr::Arc<Device>& device,
                   vk::Pipeline            copyPipeline,
                   vk::PipelineLayout      copyLayout,
                   vk::Pipeline            reducePipeline,
                   vk::PipelineLayout      reduceLayout,
                   vk::DescriptorSetLayout copySetLayout,
                   vk::DescriptorSetLayout reduceSetLayout,
                   vk::DescriptorPool      descriptorPool,
                   vk::Sampler             depthSampler,
                   std::uint32_t           frameCount);

        ~HiZPyramid();

        HiZPyramid(const HiZPyramid&)            = delete;
        HiZPyramid& operator=(const HiZPyramid&) = delete;

        void Resize(std::uint32_t width, std::uint32_t height);

        /**
         * @brief Rebuild pyramid from a depth image (shader-read layout).
         * @param frameIndex Frame-in-flight index used for descriptor sets
         */
        void Build(const skr::Arc<CommandPool>& commandPool,
                   const skr::Arc<Image>&       depthImage,
                   bool                         reverseZ,
                   std::uint32_t                frameIndex);

        [[nodiscard]] bool IsValid() const
        {
            return mImage != nullptr && mMipLevels > 0;
        }

        [[nodiscard]] bool IsReady() const { return mReady; }

        void Invalidate() { mReady = false; }

        [[nodiscard]] vk::ImageView GetSampledView() const
        {
            return mImage ? mImage->GetImageView() : vk::ImageView {};
        }

        [[nodiscard]] vk::Sampler GetSampler() const { return mDepthSampler; }

        [[nodiscard]] vk::Extent2D GetExtent() const
        {
            return { mWidth, mHeight };
        }

      private:
        void destroyMipViews();
        void writeFrameDescriptors(std::uint32_t          frame,
                                   const skr::Arc<Image>& depthImage);

        skr::Arc<Device>        mDevice;
        vk::Pipeline            mCopyPipeline;
        vk::PipelineLayout      mCopyLayout;
        vk::Pipeline            mReducePipeline;
        vk::PipelineLayout      mReduceLayout;
        vk::DescriptorSetLayout mCopySetLayout;
        vk::DescriptorSetLayout mReduceSetLayout;
        vk::DescriptorPool      mDescriptorPool;
        vk::Sampler             mDepthSampler;
        std::uint32_t           mFrameCount = 1;

        std::vector<vk::DescriptorSet> mCopySets;
        // [frame * (kMaxMipLevels-1) + (mip-1)]
        std::vector<vk::DescriptorSet> mReduceSets;

        skr::Arc<Image>            mImage;
        std::vector<vk::ImageView> mMipViews;
        std::uint32_t              mWidth     = 0;
        std::uint32_t              mHeight    = 0;
        std::uint32_t              mMipLevels = 0;
        bool                       mReady     = false;

        std::uint32_t              mPyramidGeneration = 1;
        std::vector<std::uint32_t> mFramePyramidGeneration;
        std::vector<vk::ImageView> mFrameBoundDepthView;
    };

} // namespace FREYA_NAMESPACE
