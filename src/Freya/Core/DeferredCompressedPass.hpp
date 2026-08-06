#pragma once

#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/LightService.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/Core/UniformBuffer.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    enum : std::uint32_t
    {
        DefDepthAttachment,
        DefPositionAttachment,
        DefNormalAttachment,
        DefAlbedoAttachment,
        DefEmissiveAttachment,
        DefMaterialAttachment,
    };

    enum : std::uint32_t
    {
        DefDepthPrePass,
        DefGBufferPass,
    };

    /**
     * @brief Deferred G-buffer pass with a separate fullscreen lighting pass.
     *
     * G-buffer RP: depth pre-pass + G-buffer. Final layouts are shader-read
     * so denoise and lighting can sample attachments as textures.
     */
    class DeferredCompressedPass
    {
      public:
        DeferredCompressedPass(
            const skr::Arc<Device>&       device,
            const skr::Arc<FreyaOptions>& freyaOptions,
            const skr::Arc<Surface>&      surface,
            const vk::RenderPass          gbufferRenderPass,
            const vk::RenderPass          lightingRenderPass,
            const vk::PipelineLayout      vertexPipelineLayout,
            const vk::PipelineLayout      fullscreenPipelineLayout,
            const vk::Pipeline            depthPrepassPipeline,
            const vk::Pipeline            gbufferPipeline,
            const vk::Pipeline            lightingPipeline,
            const skr::Arc<Buffer>&       uniformBuffer,
            const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
            const std::vector<vk::DescriptorSet>&       descriptorSets,
            const vk::DescriptorPool                    descriptorPool,
            const std::vector<skr::Arc<Image>>&         gbufferImages,
            const skr::Arc<Image>&                      emissiveImage,
            const skr::Arc<Image>&                      depthImage,
            const skr::Arc<Image>&                      translucentImage,
            const skr::Arc<Image>&                      opaqueImage,
            const std::vector<vk::Framebuffer>&         gbufferFramebuffers,
            const std::vector<vk::Framebuffer>&         lightingFramebuffers,
            const vk::DescriptorSetLayout               lightingSetLayout,
            const vk::DescriptorPool                    lightingDescriptorPool,
            const std::vector<vk::DescriptorSet>&       lightingSets,
            const vk::DescriptorSetLayout               samplerLayout,
            const vk::DescriptorPool                    samplerDescriptorPool,
            const vk::Sampler                           gbufferSampler,
            vk::Extent2D                                extent);

        ~DeferredCompressedPass();

        vk::RenderPass& GetRenderPass() { return mGbufferRenderPass; }

        vk::PipelineLayout& GetVertexPipelineLayout()
        {
            return mVertexPipelineLayout;
        }

        vk::PipelineLayout& GetFullscreenPipelineLayout()
        {
            return mFullscreenPipelineLayout;
        }

        vk::Pipeline& GetPipeline(std::uint32_t subpass);

        skr::Arc<Image> GetOpaqueImage() const { return mOpaqueImage; }
        skr::Arc<Image> GetTranslucentImage() const
        {
            return mTranslucentImage;
        }
        skr::Arc<Image> GetEmissiveImage() const { return mEmissiveImage; }
        skr::Arc<Image> GetDepthImage() const { return mDepthImage; }
        skr::Arc<Image> GetPositionImage() const { return mGBufferImages[0]; }
        skr::Arc<Image> GetNormalImage() const { return mGBufferImages[1]; }
        skr::Arc<Image> GetAlbedoImage() const { return mGBufferImages[2]; }
        skr::Arc<Image> GetMaterialImage() const { return mGBufferImages[4]; }

        void Begin(const skr::Arc<SwapChain>    swapChain,
                   const skr::Arc<CommandPool>& commandPool) const;

        void NextSubpass(const skr::Arc<CommandPool>& commandPool) const;

        void BindPipeline(std::uint32_t                subpass,
                          const skr::Arc<CommandPool>& commandPool,
                          std::uint32_t                frameIndex) const;

        void AdvanceSubpass(std::uint32_t                subpass,
                            const skr::Arc<CommandPool>& commandPool,
                            std::uint32_t                frameIndex) const;

        void End(const skr::Arc<CommandPool> commandPool) const;

        void BeginLighting(const skr::Arc<SwapChain>    swapChain,
                           const skr::Arc<CommandPool>& commandPool) const;

        void DrawLighting(const skr::Arc<CommandPool>& commandPool,
                          std::uint32_t                frameIndex) const;

        void EndLighting(const skr::Arc<CommandPool>& commandPool) const;

        void UpdateDirectionalShadowMask(const skr::Arc<Image>& mask,
                                         vk::Sampler            sampler);

        void UpdateProjection(const ProjectionUniformBuffer& buffer,
                              std::uint32_t                  frameIndex) const;

        vk::DescriptorSet& GetDescriptorSet(std::uint32_t frameIndex)
        {
            return mDescriptorSets[frameIndex];
        }

        vk::DescriptorSetLayout& GetSamplerLayout() { return mSamplerLayout; }

        vk::DescriptorPool& GetSamplerDescriptorPool()
        {
            return mSamplerDescriptorPool;
        }

        skr::Arc<Buffer> GetUniformBuffer() { return mUniformBuffer; }

        std::size_t GetFramebufferCount() const
        {
            return mGbufferFramebuffers.size();
        }

        vk::Framebuffer& GetFramebuffer(std::size_t index)
        {
            return mGbufferFramebuffers[index];
        }

        std::uint32_t GetCurrentSubpass() const { return mCurrentSubpass; }

        skr::Arc<Device>       mDevice;
        skr::Arc<FreyaOptions> mFreyaOptions;
        skr::Arc<Surface>      mSurface;

        vk::RenderPass mGbufferRenderPass;
        vk::RenderPass mLightingRenderPass;

      private:
        vk::PipelineLayout mVertexPipelineLayout;
        vk::PipelineLayout mFullscreenPipelineLayout;

        std::array<vk::Pipeline, 2> mGeometryPipelines;
        vk::Pipeline                mLightingPipeline;

        skr::Arc<Buffer> mUniformBuffer;

        std::vector<vk::DescriptorSetLayout> mDescriptorSetLayouts;
        std::vector<vk::DescriptorSet>       mDescriptorSets;
        vk::DescriptorPool                   mDescriptorPool;

        std::vector<skr::Arc<Image>> mGBufferImages;
        skr::Arc<Image>              mEmissiveImage;
        skr::Arc<Image>              mDepthImage;
        skr::Arc<Image>              mTranslucentImage;
        skr::Arc<Image>              mOpaqueImage;

        std::vector<vk::Framebuffer> mGbufferFramebuffers;
        std::vector<vk::Framebuffer> mLightingFramebuffers;

        vk::Extent2D mExtent;

        vk::DescriptorSetLayout        mLightingSetLayout;
        vk::DescriptorPool             mLightingDescriptorPool;
        std::vector<vk::DescriptorSet> mLightingSets;

        vk::DescriptorSetLayout mSamplerLayout;
        vk::DescriptorPool      mSamplerDescriptorPool;
        vk::Sampler             mGbufferSampler;

        mutable bool          mLabelActive    = false;
        mutable std::uint32_t mCurrentSubpass = DefDepthPrePass;

        static const char* GetSubpassLabel(std::uint32_t subpass);
    };

} // namespace FREYA_NAMESPACE
