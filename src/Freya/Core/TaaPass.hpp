#pragma once

#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/FreyaOptions.hpp"

#include <array>
#include <vector>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Temporal AA compute resolve.
     *
     * Reprojects ping-pong HDR history with velocity, rejects disocclusions
     * via depth history, and variance-clips history in YCoCg before blend.
     * Bloom should continue to sample pre-TAA Scene Color.
     *
     * History is kept per flight slot (one ping-pong pair per frame in
     * flight): frames overlap on the GPU, so a single global pair would let
     * a later frame record barriers/reads against history that an earlier
     * frame has not written yet.
     */
    class TaaPass
    {
      public:
        TaaPass(
            const skr::Arc<Device>&                              device,
            const skr::Arc<FreyaOptions>&                        freyaOptions,
            vk::PipelineLayout                                   pipelineLayout,
            vk::Pipeline                                         pipeline,
            vk::DescriptorSetLayout                              setLayout,
            vk::DescriptorPool                                   descriptorPool,
            const std::vector<std::array<vk::DescriptorSet, 2>>& descriptorSets,
            const std::vector<std::array<skr::Arc<Image>, 2>>&   historyImages,
            const std::vector<std::array<skr::Arc<Image>, 2>>&
                          depthHistoryImages,
            vk::Sampler   colorSampler,
            vk::Sampler   nearestSampler,
            vk::Extent2D  extent,
            std::uint32_t frameCount);

        ~TaaPass();

        [[nodiscard]] std::uint32_t FrameCount() const
        {
            return static_cast<std::uint32_t>(mHistoryImages.size());
        }

        /// Resolved HDR after TAA (this slot's ping-pong entry last written).
        skr::Arc<Image> GetOutputImage(std::uint32_t frameIndex) const
        {
            const auto slot = frameIndex % FrameCount();
            return mHistoryImages[slot][1u - mWriteIndex[slot]];
        }

        void ResetHistory();

        void Dispatch(const skr::Arc<CommandPool>& commandPool,
                      const skr::Arc<Image>&       sceneColor,
                      const skr::Arc<Image>&       velocity,
                      const skr::Arc<Image>&       depth,
                      std::uint32_t                frameIndex) const;

      private:
        struct BoundViews
        {
            vk::ImageView scene    = {};
            vk::ImageView velocity = {};
            vk::ImageView depth    = {};
        };

        void ensureSceneDescriptors(const skr::Arc<Image>& sceneColor,
                                    const skr::Arc<Image>& velocity,
                                    const skr::Arc<Image>& depth,
                                    std::uint32_t          frameIndex) const;

        skr::Arc<Device>       mDevice;
        skr::Arc<FreyaOptions> mFreyaOptions;

        vk::PipelineLayout                            mPipelineLayout;
        vk::Pipeline                                  mPipeline;
        vk::DescriptorSetLayout                       mSetLayout;
        vk::DescriptorPool                            mDescriptorPool;
        std::vector<std::array<vk::DescriptorSet, 2>> mDescriptorSets;
        std::vector<std::array<skr::Arc<Image>, 2>>   mHistoryImages;
        std::vector<std::array<skr::Arc<Image>, 2>>   mDepthHistoryImages;
        vk::Sampler                                   mColorSampler;
        vk::Sampler                                   mNearestSampler;
        vk::Extent2D                                  mExtent;

        mutable std::vector<std::uint32_t> mWriteIndex;
        mutable std::vector<char>          mHistoryValid;
        mutable std::vector<BoundViews>    mBoundViews;
    };
} // namespace FREYA_NAMESPACE
