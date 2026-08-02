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

#include <variant>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Attachment indices for deferred Gbuffer+Lighting pass.
     */
    enum : std::uint32_t
    {
        DefDepthAttachment,       ///< Depth attachment
        DefPositionAttachment,    ///< G-buffer world position
        DefNormalAttachment,      ///< G-buffer normal
        DefAlbedoAttachment,      ///< G-buffer albedo
        DefEmissiveAttachment,    ///< G-buffer emissive (for bloom)
        DefMaterialAttachment,    ///< G-buffer material (metalness)
        DefTranslucentAttachment, ///< Translucent objects buffer
        DefOpaqueAttachment,      ///< Opaque lit result buffer
    };

    /**
     * @brief Subpass indices for deferred Gbuffer+Lighting pipeline.
     */
    enum : std::uint32_t
    {
        DefDepthPrePass,    ///< Depth pre-pass (subpass 0)
        DefGBufferPass,     ///< G-buffer generation (subpass 1)
        DefLightingPass,    ///< Lighting calculation (subpass 2)
        DefTranslucentPass, ///< Translucent rendering (subpass 3)
    };

    /**
     * @brief Deferred Gbuffer+Lighting render pass.
     *
     * Manages 4 subpasses: depth pre-pass, G-buffer, lighting, translucent.
     * Owns G-buffer images and framebuffers.
     * Bloom and composite are handled in separate passes.
     */
    class DeferredCompressedPass
    {
      public:
        DeferredCompressedPass(
            const skr::Arc<Device>&       device,
            const skr::Arc<FreyaOptions>& freyaOptions,
            const skr::Arc<Surface>&      surface,
            const vk::RenderPass          renderPass,
            const vk::PipelineLayout      vertexPipelineLayout,
            const vk::PipelineLayout      fullscreenPipelineLayout,
            const vk::Pipeline            depthPrepassPipeline,
            const vk::Pipeline            gbufferPipeline,
            const vk::Pipeline            lightingPipeline,
            const vk::Pipeline            translucentPipeline,
            const skr::Arc<Buffer>&       uniformBuffer,
            const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
            const std::vector<vk::DescriptorSet>&       descriptorSets,
            const vk::DescriptorPool                    descriptorPool,
            const std::vector<skr::Arc<Image>>&         gbufferImages,
            const skr::Arc<Image>&                      emissiveImage,
            const skr::Arc<Image>&                      depthImage,
            const skr::Arc<Image>&                      translucentImage,
            const skr::Arc<Image>&                      opaqueImage,
            const std::vector<vk::Framebuffer>&         framebuffers,
            const vk::DescriptorSetLayout               inputAttachmentLayout,
            const vk::DescriptorPool                    inputAttachmentPool,
            const std::vector<vk::DescriptorSet>&       lightingInputSets,
            const vk::DescriptorSetLayout               samplerLayout,
            const vk::DescriptorPool                    samplerDescriptorPool,
            vk::Extent2D                                extent);

        ~DeferredCompressedPass();

        vk::RenderPass& GetRenderPass() { return mRenderPass; }

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

        void Begin(const skr::Arc<SwapChain>    swapChain,
                   const skr::Arc<CommandPool>& commandPool) const;

        void NextSubpass(const skr::Arc<CommandPool>& commandPool) const;

        void BindPipeline(std::uint32_t                subpass,
                          const skr::Arc<CommandPool>& commandPool,
                          std::uint32_t                frameIndex) const;

        void AdvanceSubpass(std::uint32_t                subpass,
                            const skr::Arc<CommandPool>& commandPool,
                            std::uint32_t                frameIndex) const;

        void DrawFullscreenTriangle(
            const skr::Arc<CommandPool>& commandPool) const;

        void End(const skr::Arc<CommandPool> commandPool) const;

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

        std::size_t GetFramebufferCount() const { return mFramebuffers.size(); }

        vk::Framebuffer& GetFramebuffer(std::size_t index)
        {
            return mFramebuffers[index];
        }

        std::uint32_t GetCurrentSubpass() const { return mCurrentSubpass; }

        skr::Arc<Device>       mDevice;
        skr::Arc<FreyaOptions> mFreyaOptions;
        skr::Arc<Surface>      mSurface;

        vk::RenderPass mRenderPass;

      private:
        vk::PipelineLayout mVertexPipelineLayout;
        vk::PipelineLayout mFullscreenPipelineLayout;

        std::array<vk::Pipeline, 4> mPipelines;

        skr::Arc<Buffer> mUniformBuffer;

        std::vector<vk::DescriptorSetLayout> mDescriptorSetLayouts;
        std::vector<vk::DescriptorSet>       mDescriptorSets;
        vk::DescriptorPool                   mDescriptorPool;

        // G-buffer and intermediate images
        std::vector<skr::Arc<Image>> mGBufferImages;
        skr::Arc<Image>              mEmissiveImage;
        skr::Arc<Image>              mDepthImage;
        skr::Arc<Image>              mTranslucentImage;
        skr::Arc<Image>              mOpaqueImage;

        // Framebuffers (one per swapchain image)
        std::vector<vk::Framebuffer> mFramebuffers;

        vk::Extent2D mExtent;

        // Input attachment descriptor resources (for lighting)
        vk::DescriptorSetLayout        mInputAttachmentLayout;
        vk::DescriptorPool             mInputAttachmentPool;
        std::vector<vk::DescriptorSet> mLightingInputSets;

        // Sampler descriptor resources (sampler pool shared with forward pass)
        vk::DescriptorSetLayout mSamplerLayout;
        vk::DescriptorPool      mSamplerDescriptorPool;

        // Debug label state
        mutable bool mLabelActive = false;

        // Current subpass tracking
        mutable std::uint32_t mCurrentSubpass = DefDepthPrePass;

        static const char* GetSubpassLabel(std::uint32_t subpass);
    };

} // namespace FREYA_NAMESPACE
