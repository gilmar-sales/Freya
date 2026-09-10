#pragma once

#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/LightService.hpp"
#include "Freya/Core/ShadowPass.hpp"
#include "Freya/FreyaOptions.hpp"

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * Half-resolution directional CSM visibility (R8). Lighting samples this
     * instead of running Poisson taps per fullscreen pixel.
     */
    class ShadowMaskPass
    {
      public:
        ShadowMaskPass(const skr::Arc<Device>&       device,
                       const skr::Arc<FreyaOptions>& freyaOptions,
                       vk::PipelineLayout            pipelineLayout,
                       vk::Pipeline                  pipeline,
                       vk::DescriptorSetLayout       setLayout,
                       vk::DescriptorPool            descriptorPool,
                       vk::DescriptorSet             set,
                       vk::Sampler                   nearestSampler,
                       vk::Sampler                   linearSampler,
                       const skr::Arc<Image>&        maskImage,
                       vk::Extent2D                  fullExtent,
                       vk::Extent2D                  maskExtent);

        ~ShadowMaskPass();

        [[nodiscard]] skr::Arc<Image> GetOutputImage() const
        {
            return mMaskImage;
        }

        void Dispatch(const skr::Arc<CommandPool>& commandPool,
                      const skr::Arc<Image>&       depthImage,
                      const skr::Arc<Image>&       normalImage,
                      const skr::Arc<ShadowPass>&  shadowPass,
                      const LightService&          lights,
                      const glm::mat4&             view,
                      const glm::mat4&             unjitteredProjection,
                      bool                         reverseZ,
                      std::uint32_t                frameIndex) const;

      private:
        void ensureDescriptors(const skr::Arc<Image>&      depthImage,
                               const skr::Arc<Image>&      normalImage,
                               const skr::Arc<ShadowPass>& shadowPass,
                               const LightService&         lights,
                               std::uint32_t               frameIndex) const;

        skr::Arc<Device>       mDevice;
        skr::Arc<FreyaOptions> mFreyaOptions;

        vk::PipelineLayout      mPipelineLayout;
        vk::Pipeline            mPipeline;
        vk::DescriptorSetLayout mSetLayout;
        vk::DescriptorPool      mDescriptorPool;
        vk::DescriptorSet       mSet;

        vk::Sampler     mNearestSampler;
        vk::Sampler     mLinearSampler;
        skr::Arc<Image> mMaskImage;
        vk::Extent2D    mFullExtent;
        vk::Extent2D    mMaskExtent;

        mutable vk::ImageView mBoundDepthView   = {};
        mutable vk::ImageView mBoundNormalView  = {};
        mutable vk::ImageView mBoundCascadeView = {};
        mutable bool          mStaticBound      = false;
        mutable std::uint32_t mBoundFrame       = ~0u;
    };
} // namespace FREYA_NAMESPACE
