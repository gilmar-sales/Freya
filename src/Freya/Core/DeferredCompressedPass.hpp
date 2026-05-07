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
            const Ref<Device>&       device,
            const Ref<FreyaOptions>& freyaOptions,
            const Ref<Surface>&      surface,
            const vk::RenderPass     renderPass,
            const vk::PipelineLayout vertexPipelineLayout,
            const vk::PipelineLayout fullscreenPipelineLayout,
            const vk::Pipeline       depthPrepassPipeline,
            const vk::Pipeline       gbufferPipeline,
            const vk::Pipeline       lightingPipeline,
            const vk::Pipeline       translucentPipeline,
            const Ref<Buffer>&       uniformBuffer,
            const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
            const std::vector<vk::DescriptorSet>&       descriptorSets,
            const vk::DescriptorPool                    descriptorPool,
            const std::vector<Ref<Image>>&              gbufferImages,
            const Ref<Image>&                           emissiveImage,
            const Ref<Image>&                           depthImage,
            const Ref<Image>&                           translucentImage,
            const Ref<Image>&                           opaqueImage,
            const std::vector<vk::Framebuffer>&         framebuffers,
            const vk::DescriptorSetLayout               inputAttachmentLayout,
            const vk::DescriptorPool                    inputAttachmentPool,
            const vk::DescriptorSet                     lightingInputSet,
            const vk::DescriptorSetLayout               samplerLayout,
            const vk::DescriptorPool                    samplerDescriptorPool);

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

        Ref<Image> GetOpaqueImage() const { return mOpaqueImage; }
        Ref<Image> GetTranslucentImage() const { return mTranslucentImage; }
        Ref<Image> GetEmissiveImage() const { return mEmissiveImage; }

        void Begin(const Ref<SwapChain>    swapChain,
                   const Ref<CommandPool>& commandPool) const;

        void NextSubpass(const Ref<CommandPool>& commandPool) const;

        void BindPipeline(std::uint32_t           subpass,
                          const Ref<CommandPool>& commandPool,
                          std::uint32_t           frameIndex) const;

        void AdvanceSubpass(std::uint32_t           subpass,
                            const Ref<CommandPool>& commandPool,
                            std::uint32_t           frameIndex) const;

        void DrawFullscreenTriangle(const Ref<CommandPool>& commandPool) const;

        void End(const Ref<CommandPool> commandPool) const;

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

        Ref<Buffer> GetUniformBuffer() { return mUniformBuffer; }

        std::size_t GetFramebufferCount() const { return mFramebuffers.size(); }

        vk::Framebuffer& GetFramebuffer(std::size_t index)
        {
            return mFramebuffers[index];
        }

        std::uint32_t GetCurrentSubpass() const { return mCurrentSubpass; }

        Ref<Device>       mDevice;
        Ref<FreyaOptions> mFreyaOptions;
        Ref<Surface>      mSurface;

        vk::RenderPass mRenderPass;

      private:
        vk::PipelineLayout mVertexPipelineLayout;
        vk::PipelineLayout mFullscreenPipelineLayout;

        std::array<vk::Pipeline, 4> mPipelines;

        Ref<Buffer> mUniformBuffer;

        std::vector<vk::DescriptorSetLayout> mDescriptorSetLayouts;
        std::vector<vk::DescriptorSet>       mDescriptorSets;
        vk::DescriptorPool                   mDescriptorPool;

        // G-buffer and intermediate images
        std::vector<Ref<Image>> mGBufferImages;
        Ref<Image>              mEmissiveImage;
        Ref<Image>              mDepthImage;
        Ref<Image>              mTranslucentImage;
        Ref<Image>              mOpaqueImage;

        // Framebuffers (one per swapchain image)
        std::vector<vk::Framebuffer> mFramebuffers;

        // Input attachment descriptor resources (for lighting)
        vk::DescriptorSetLayout mInputAttachmentLayout;
        vk::DescriptorPool      mInputAttachmentPool;
        vk::DescriptorSet       mLightingInputSet;

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
