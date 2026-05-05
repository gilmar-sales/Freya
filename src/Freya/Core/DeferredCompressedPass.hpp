#pragma once

#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/Core/UniformBuffer.hpp"
#include "Freya/FreyaOptions.hpp"

#include <variant>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Attachment indices for deferred rendering pass.
     */
    enum : std::uint32_t
    {
        DeferredBackAttachment,          ///< Back buffer color attachment
        DeferredDepthAttachment,         ///< Depth attachment
        DeferredPositionAttachment,      ///< G-buffer world position
        DeferredNormalAttachment,        ///< G-buffer normal
        DeferredAlbedoAttachment,        ///< G-buffer albedo + specular
        DeferredTranslucentAttachment,   ///< Translucent objects buffer
        DeferredOpaqueAttachment         ///< Opaque lit result buffer
    };

    /**
     * @brief Subpass indices for deferred rendering pipeline.
     */
    enum : std::uint32_t
    {
        DeferredDepthPrePass,    ///< Depth pre-pass (subpass 0)
        DeferredGBufferPass,     ///< G-buffer generation (subpass 1)
        DeferredLightingPass,    ///< Lighting calculation (subpass 2)
        DeferredTranslucentPass, ///< Translucent rendering (subpass 3)
        DeferredCompositePass    ///< Final compositing (subpass 4)
    };

    /**
     * @brief Deferred rendering pass with G-buffer attachments and
     * multi-subpass pipeline.
     *
     * Manages a deferred render pass with 7 attachments and 5 subpasses:
     * Depth pre-pass, G-buffer, Lighting, Translucent, Composite.
     * Owns G-buffer images, framebuffers, and per-subpass graphics pipelines.
     */
    class DeferredCompressedPass
    {
      public:
        DeferredCompressedPass(
            const Ref<Device>&               device,
            const Ref<FreyaOptions>&         freyaOptions,
            const Ref<Surface>&              surface,
            const vk::RenderPass             renderPass,
            const vk::PipelineLayout         vertexPipelineLayout,
            const vk::PipelineLayout         fullscreenPipelineLayout,
            const vk::Pipeline               depthPrepassPipeline,
            const vk::Pipeline               gbufferPipeline,
            const vk::Pipeline               lightingPipeline,
            const vk::Pipeline               translucentPipeline,
            const vk::Pipeline               compositePipeline,
            const Ref<Buffer>&               uniformBuffer,
            const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
            const std::vector<vk::DescriptorSet>&       descriptorSets,
            const vk::DescriptorPool                    descriptorPool,
            const std::vector<Ref<Image>>&              gbufferImages,
            const Ref<Image>&                           depthImage,
            const Ref<Image>&                           translucentImage,
            const Ref<Image>&                           opaqueImage,
            const std::vector<vk::Framebuffer>&         framebuffers,
            const vk::DescriptorSetLayout inputAttachmentLayout,
            const vk::DescriptorPool      inputAttachmentPool,
            const vk::DescriptorSet       lightingInputSet,
            const vk::DescriptorSet       compositeInputSet,
            const vk::DescriptorSetLayout samplerLayout,
            const vk::DescriptorPool      samplerDescriptorPool);

        ~DeferredCompressedPass();

        /**
         * @brief Returns the underlying render pass handle.
         */
        vk::RenderPass& GetRenderPass() { return mRenderPass; }

        /**
         * @brief Returns the vertex pipeline layout (for depth/gbuffer/
         * translucent subpasses).
         */
        vk::PipelineLayout& GetVertexPipelineLayout()
        {
            return mVertexPipelineLayout;
        }

        /**
         * @brief Returns the fullscreen pipeline layout (for lighting/composite
         * subpasses).
         */
        vk::PipelineLayout& GetFullscreenPipelineLayout()
        {
            return mFullscreenPipelineLayout;
        }

        /**
         * @brief Returns a specific pipeline by subpass index.
         */
        vk::Pipeline& GetPipeline(std::uint32_t subpass);

        /**
         * @brief Begins the deferred render pass with all clear values.
         * Puts the command buffer in subpass 0 (depth pre-pass).
         */
        void Begin(const Ref<SwapChain>  swapChain,
                   const Ref<CommandPool> commandPool) const;

        /**
         * @brief Advances to the next subpass.
         */
        void NextSubpass(const Ref<CommandPool> commandPool) const;

        /**
         * @brief Binds the pipeline for the given subpass index.
         * Also binds the UBO descriptor set if applicable.
         */
        void BindPipeline(std::uint32_t             subpass,
                          const Ref<CommandPool>&   commandPool,
                          std::uint32_t             frameIndex) const;

        /**
         * @brief Convenience: calls NextSubpass then BindPipeline.
         */
        void AdvanceSubpass(std::uint32_t           subpass,
                            const Ref<CommandPool>& commandPool,
                            std::uint32_t           frameIndex) const;

        /**
         * @brief Draws a fullscreen triangle for lighting/composite passes.
         * Uses vertex-less draw (gl_VertexIndex in the vertex shader).
         */
        void DrawFullscreenTriangle(
            const Ref<CommandPool>& commandPool) const;

        /**
         * @brief Ends the render pass.
         */
        void End(const Ref<CommandPool> commandPool) const;

        /**
         * @brief Updates the projection uniform buffer for a frame index.
         */
        void UpdateProjection(const ProjectionUniformBuffer& buffer,
                              std::uint32_t frameIndex) const;

        /**
         * @brief Returns the UBO descriptor set for a given frame.
         */
        vk::DescriptorSet& GetDescriptorSet(std::uint32_t frameIndex)
        {
            return mDescriptorSets[frameIndex];
        }

        /**
         * @brief Returns the sampler descriptor set layout (for external
         * texture binding).
         */
        vk::DescriptorSetLayout& GetSamplerLayout()
        {
            return mSamplerLayout;
        }

        /**
         * @brief Returns the sampler descriptor pool (for external texture
         * binding).
         */
        vk::DescriptorPool& GetSamplerDescriptorPool()
        {
            return mSamplerDescriptorPool;
        }

        /**
         * @brief Returns the shared uniform buffer.
         */
        Ref<Buffer> GetUniformBuffer() { return mUniformBuffer; }

        /**
         * @brief Returns the framebuffer count (matches swapchain image count).
         */
        std::size_t GetFramebufferCount() const { return mFramebuffers.size(); }

        /**
         * @brief Returns a framebuffer by index.
         */
        vk::Framebuffer& GetFramebuffer(std::size_t index)
        {
            return mFramebuffers[index];
        }

        Ref<Device>       mDevice;
        Ref<FreyaOptions> mFreyaOptions;
        Ref<Surface>      mSurface;

        vk::RenderPass mRenderPass;

      private:
        vk::PipelineLayout mVertexPipelineLayout;
        vk::PipelineLayout mFullscreenPipelineLayout;

        std::array<vk::Pipeline, 5> mPipelines;

        Ref<Buffer> mUniformBuffer;

        std::vector<vk::DescriptorSetLayout> mDescriptorSetLayouts;
        std::vector<vk::DescriptorSet>       mDescriptorSets;
        vk::DescriptorPool                   mDescriptorPool;

        // G-buffer and intermediate images
        std::vector<Ref<Image>> mGBufferImages; // position, normal, albedo
        Ref<Image> mDepthImage;
        Ref<Image> mTranslucentImage;
        Ref<Image> mOpaqueImage;

        // Framebuffers (one per swapchain image)
        std::vector<vk::Framebuffer> mFramebuffers;

        // Input attachment descriptor resources (for lighting/composite)
        vk::DescriptorSetLayout mInputAttachmentLayout;
        vk::DescriptorPool      mInputAttachmentPool;
        vk::DescriptorSet       mLightingInputSet;
        vk::DescriptorSet       mCompositeInputSet;

        // Sampler descriptor resources (sampler pool shared with forward pass)
        vk::DescriptorSetLayout mSamplerLayout;
        vk::DescriptorPool      mSamplerDescriptorPool;
    };

} // namespace FREYA_NAMESPACE
