#pragma once

#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Temporal AA compute resolve (neighborhood-clamped history blend).
     *
     * Reads lit Scene Color + velocity, blends with ping-pong history, and
     * exposes the resolved HDR image for composite. Bloom should sample
     * pre-TAA Scene Color.
     */
    class TaaPass
    {
      public:
        TaaPass(const skr::Arc<Device>&                 device,
                const skr::Arc<FreyaOptions>&           freyaOptions,
                vk::PipelineLayout                      pipelineLayout,
                vk::Pipeline                            pipeline,
                vk::DescriptorSetLayout                 setLayout,
                vk::DescriptorPool                      descriptorPool,
                const std::array<vk::DescriptorSet, 2>& descriptorSets,
                const std::array<skr::Arc<Image>, 2>&   historyImages,
                vk::Sampler                             sampler,
                vk::Extent2D                            extent);

        ~TaaPass();

        /// Resolved HDR after TAA (history ping-pong slot last written).
        skr::Arc<Image> GetOutputImage() const
        {
            return mHistoryImages[1u - mWriteIndex];
        }

        void ResetHistory() { mHistoryValid = false; }

        void Dispatch(const skr::Arc<CommandPool>& commandPool,
                      const skr::Arc<Image>&       sceneColor,
                      const skr::Arc<Image>&       velocity) const;

      private:
        void updateDescriptors(const skr::Arc<Image>& sceneColor,
                               const skr::Arc<Image>& velocity) const;

        skr::Arc<Device>       mDevice;
        skr::Arc<FreyaOptions> mFreyaOptions;

        vk::PipelineLayout               mPipelineLayout;
        vk::Pipeline                     mPipeline;
        vk::DescriptorSetLayout          mSetLayout;
        vk::DescriptorPool               mDescriptorPool;
        std::array<vk::DescriptorSet, 2> mDescriptorSets;
        std::array<skr::Arc<Image>, 2>   mHistoryImages;
        vk::Sampler                      mSampler;
        vk::Extent2D                     mExtent;

        mutable std::uint32_t mWriteIndex   = 0;
        mutable bool          mHistoryValid = false;
    };
} // namespace FREYA_NAMESPACE
