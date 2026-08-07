#pragma once

#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/FreyaOptions.hpp"

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    struct alignas(256) SsaoCameraBuffer
    {
        alignas(64) glm::mat4 invProjection {};
        alignas(64) glm::mat4 view {};
        alignas(64) glm::mat4 projection {};
        alignas(16) glm::vec4 params {}; ///< radius, bias, power, intensity
        alignas(16) glm::vec4 res {};    ///< invRes.xy, noiseScale, reverseZ
    };

    /**
     * Half-resolution SSAO + bilateral blur.
     * Output is R8 AO sampled by lighting (binding 15) with linear upsample.
     */
    class SsaoPass
    {
      public:
        SsaoPass(const skr::Arc<Device>&       device,
                 const skr::Arc<FreyaOptions>& freyaOptions,
                 vk::PipelineLayout            ssaoPipelineLayout,
                 vk::Pipeline                  ssaoPipeline,
                 vk::PipelineLayout            blurPipelineLayout,
                 vk::Pipeline                  blurPipeline,
                 vk::DescriptorSetLayout       ssaoSetLayout,
                 vk::DescriptorSetLayout       blurSetLayout,
                 vk::DescriptorPool            descriptorPool,
                 vk::DescriptorSet             ssaoSet,
                 vk::DescriptorSet             blurSetH,
                 vk::DescriptorSet             blurSetV,
                 const skr::Arc<Buffer>&       cameraBuffer,
                 vk::Sampler                   nearestSampler,
                 vk::Sampler                   noiseSampler,
                 vk::Sampler                   linearSampler,
                 const skr::Arc<Image>&        ssaoRawImage,
                 const std::array<skr::Arc<Image>, 2>& blurImages,
                 const skr::Arc<Image>&        noiseImage,
                 vk::Extent2D                  fullExtent,
                 vk::Extent2D                  ssaoExtent);

        ~SsaoPass();

        skr::Arc<Image> GetOutputImage() const { return mBlurImages[1]; }

        void Dispatch(const skr::Arc<CommandPool>& commandPool,
                      const skr::Arc<Image>&       depthImage,
                      const skr::Arc<Image>&       normalImage,
                      const glm::mat4&             view,
                      const glm::mat4&             unjitteredProjection,
                      bool                         reverseZ,
                      float radius    = 1.25f,
                      float bias      = 0.04f,
                      float power     = 2.0f,
                      float intensity = 1.35f) const;

      private:
        void barrierColor(const skr::Arc<CommandPool>& commandPool,
                          vk::Image                    image,
                          vk::ImageLayout              oldLayout,
                          vk::ImageLayout              newLayout,
                          vk::AccessFlags              srcAccess,
                          vk::AccessFlags              dstAccess,
                          vk::PipelineStageFlags       srcStage,
                          vk::PipelineStageFlags       dstStage) const;

        skr::Arc<Device>       mDevice;
        skr::Arc<FreyaOptions> mFreyaOptions;

        vk::PipelineLayout mSsaoPipelineLayout;
        vk::Pipeline       mSsaoPipeline;
        vk::PipelineLayout mBlurPipelineLayout;
        vk::Pipeline       mBlurPipeline;

        vk::DescriptorSetLayout mSsaoSetLayout;
        vk::DescriptorSetLayout mBlurSetLayout;
        vk::DescriptorPool      mDescriptorPool;
        vk::DescriptorSet       mSsaoSet;
        vk::DescriptorSet       mBlurSetH;
        vk::DescriptorSet       mBlurSetV;

        skr::Arc<Buffer>               mCameraBuffer;
        vk::Sampler                    mNearestSampler;
        vk::Sampler                    mNoiseSampler;
        vk::Sampler                    mLinearSampler;
        skr::Arc<Image>                mSsaoRawImage;
        std::array<skr::Arc<Image>, 2> mBlurImages;
        skr::Arc<Image>                mNoiseImage;
        vk::Extent2D                   mFullExtent;
        vk::Extent2D                   mSsaoExtent;
    };
} // namespace FREYA_NAMESPACE
