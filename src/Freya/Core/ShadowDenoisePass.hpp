#pragma once

#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/FreyaOptions.hpp"

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

namespace FREYA_NAMESPACE
{
    struct ShadowDenoiseCameraUBO
    {
        alignas(16) glm::mat4 invViewProj { 1.0f };
        alignas(16) glm::vec4 viewPos { 0.0f };
        alignas(16) glm::vec4 cameraForward { 0.0f, 0.0f, -1.0f, 0.0f };
        alignas(16) glm::vec4 lightDirection { 0.0f, -1.0f, 0.0f, 0.0f };
    };

    struct ShadowDenoisePushConstants
    {
        glm::vec4 axisAndParams { 1.0f, 0.0f, 4.0f, 0.0f };
        glm::vec4 sigmas { 0.02f, 32.0f, 0.0f, 0.0f };
    };

    enum : std::uint32_t
    {
        ShadowDenoiseMaskPass,
        ShadowDenoiseBlurHPass,
        ShadowDenoiseBlurVPass,
        ShadowDenoiseUpsamplePass,
        ShadowDenoisePassCount
    };

    class ShadowDenoisePass
    {
      public:
        ShadowDenoisePass(
            const skr::Arc<Device>&                     device,
            const skr::Arc<FreyaOptions>&               freyaOptions,
            const skr::Arc<Surface>&                    surface,
            vk::Extent2D                                fullExtent,
            vk::Extent2D                                halfExtent,
            const std::array<vk::RenderPass, 4>&        renderPasses,
            vk::PipelineLayout                          maskLayout,
            vk::PipelineLayout                          blurLayout,
            vk::PipelineLayout                          upsampleLayout,
            const std::array<vk::Pipeline, 4>&          pipelines,
            const skr::Arc<Image>&                      halfMaskImage,
            const skr::Arc<Image>&                      blurTempImage,
            const skr::Arc<Image>&                      resultImage,
            const std::array<vk::Framebuffer, 4>&       framebuffers,
            vk::DescriptorPool                          descriptorPool,
            const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
            const std::array<vk::DescriptorSet, 4>&     descriptorSets,
            const skr::Arc<Buffer>&                     cameraBuffer,
            vk::Sampler                                 sampler);

        ~ShadowDenoisePass();

        void UpdateDescriptors(const skr::Arc<Image>& depthImage,
                               const skr::Arc<Image>& normalImage,
                               vk::ImageView          cascadeCompareView,
                               vk::ImageView          cascadeDepthView,
                               vk::Sampler            compareSampler,
                               vk::Sampler            depthSampler,
                               const skr::Arc<Buffer>& shadowUniformBuffer);

        void Render(const skr::Arc<CommandPool>& commandPool,
                    const glm::mat4&             invViewProj,
                    const glm::vec3&             viewPos,
                    const glm::vec3&             cameraForward,
                    const glm::vec3&             lightDirection);

        skr::Arc<Image> GetResult() const { return mResultImage; }

        vk::Sampler GetSampler() const { return mSampler; }

      private:
        void drawPass(const skr::Arc<CommandPool>&     commandPool,
                      std::uint32_t                    passIndex,
                      vk::Extent2D                     extent,
                      const ShadowDenoisePushConstants& push) const;

        skr::Arc<Device>       mDevice;
        skr::Arc<FreyaOptions> mFreyaOptions;
        skr::Arc<Surface>      mSurface;

        vk::Extent2D mFullExtent;
        vk::Extent2D mHalfExtent;

        std::array<vk::RenderPass, 4>  mRenderPasses;
        vk::PipelineLayout             mMaskLayout;
        vk::PipelineLayout             mBlurLayout;
        vk::PipelineLayout             mUpsampleLayout;
        std::array<vk::Pipeline, 4>    mPipelines;
        skr::Arc<Image>                mHalfMaskImage;
        skr::Arc<Image>                mBlurTempImage;
        skr::Arc<Image>                mResultImage;
        std::array<vk::Framebuffer, 4> mFramebuffers;

        vk::DescriptorPool                   mDescriptorPool;
        std::vector<vk::DescriptorSetLayout> mDescriptorSetLayouts;
        std::array<vk::DescriptorSet, 4>     mDescriptorSets;

        skr::Arc<Buffer> mCameraBuffer;
        vk::Sampler      mSampler;
    };
} // namespace FREYA_NAMESPACE
