#pragma once

#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/Core/UniformBuffer.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Attachment indices for render pass.
     */
    enum : std::uint32_t
    {
        ColorAttachment,       ///< Color attachment index
        DepthAttachment,       ///< Depth attachment index
        ColorResolveAttachment ///< MSAA resolve attachment index
    };

    /**
     * @brief Encapsulates a complete render pass with pipeline and descriptors.
     *
     * Manages a Vulkan render pass, graphics pipeline, descriptor sets,
     * and uniform buffer for projection matrices. Handles begin/end
     * render pass operations and descriptor set binding.
     *
     * @param device              Device reference
     * @param freyaOptions        Freya options for clear values and sample
     * count
     * @param renderPass          Vulkan render pass handle
     * @param pipelineLayout      Pipeline layout handle
     * @param graphicsPipeline    Graphics pipeline handle
     * @param uniformBuffer       Uniform buffer for projection matrices
     * @param descriptorSetLayouts Descriptor set layout vector
     * @param descriptorSets      Allocated descriptor sets
     * @param descriptorPool      Descriptor pool
     * @param samplerLayout       Sampler descriptor set layout
     * @param samplerDescriptorPool Sampler descriptor pool
     * @param fallbackSamplerSet  Fallback descriptor set for set 1 (samplers)
     * @param fallbackImage       Fallback 1x1 white image
     * @param fallbackImageMemory Fallback image memory
     * @param fallbackImageView   Fallback image view
     * @param fallbackSampler     Fallback sampler
     */
    class RenderPass
    {
      public:
        RenderPass(
            const Ref<Device>& device, const Ref<FreyaOptions>& freyaOptions,
            const vk::RenderPass                        renderPass,
            const vk::PipelineLayout                    pipelineLayout,
            const vk::Pipeline                          graphicsPipeline,
            const Ref<Buffer>&                          uniformBuffer,
            const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
            const std::vector<vk::DescriptorSet>&       descriptorSets,
            const vk::DescriptorPool                    descriptorPool,
            const vk::DescriptorSetLayout               samplerLayout,
            const vk::DescriptorPool                    samplerDescriptorPool,
            const vk::DescriptorSet                     fallbackSamplerSet,
            const vk::Image                             fallbackImage,
            const vk::DeviceMemory                      fallbackImageMemory,
            const vk::ImageView                         fallbackImageView,
            const vk::Sampler                           fallbackSampler,
            const vk::Image                             emissiveFallbackImage,
            const vk::DeviceMemory                      emissiveFallbackMemory,
            const vk::ImageView emissiveFallbackImageView,
            const vk::Sampler   emissiveFallbackSampler) :
            mDevice(device), mFreyaOptions(freyaOptions),
            mRenderPass(renderPass), mPipelineLayout(pipelineLayout),
            mGraphicsPipeline(graphicsPipeline), mUniformBuffer(uniformBuffer),
            mDescriptorSetLayouts(descriptorSetLayouts),
            mDescriptorSets(descriptorSets), mDescriptorPool(descriptorPool),
            mSamplerLayout(samplerLayout),
            mSamplerDescriptorPool(samplerDescriptorPool),
            mFallbackSamplerSet(fallbackSamplerSet),
            mFallbackImage(fallbackImage),
            mFallbackImageMemory(fallbackImageMemory),
            mFallbackImageView(fallbackImageView),
            mFallbackSampler(fallbackSampler),
            mEmissiveFallbackImage(emissiveFallbackImage),
            mEmissiveFallbackMemory(emissiveFallbackMemory),
            mEmissiveFallbackImageView(emissiveFallbackImageView),
            mEmissiveFallbackSampler(emissiveFallbackSampler)
        {
        }

        ~RenderPass();

        /**
         * @brief Returns the underlying render pass handle.
         */
        vk::RenderPass& Get() { return mRenderPass; }

        /**
         * @brief Returns the pipeline layout handle.
         */
        vk::PipelineLayout& GetPipelineLayout() { return mPipelineLayout; }

        /**
         * @brief Begins the render pass with clear values and binds pipeline.
         * @param swapChain   Swapchain for framebuffer access
         * @param commandPool Command pool for current command buffer
         */
        void Begin(const Ref<SwapChain> swapChain,
                   const Ref<CommandPool>
                       commandPool) const;

        /**
         * @brief Ends the render pass.
         * @param commandPool Command pool for current command buffer
         */
        void End(const Ref<CommandPool> commandPool) const;

        /**
         * @brief Binds descriptor set at pipeline layout binding 0.
         * @param commandPool Command pool for current command buffer
         * @param frameIndex  Frame index for descriptor set selection
         */
        void BindDescriptorSet(const Ref<CommandPool>& commandPool,
                               std::uint32_t           frameIndex) const;

        /**
         * @brief Updates projection uniform buffer for a given frame index.
         * @param buffer     Projection data to upload
         * @param frameIndex Frame index for offset calculation
         */
        void UpdateProjection(const ProjectionUniformBuffer& buffer,
                              std::uint32_t                  frameIndex) const;

        /**
         * @brief Returns the sampler descriptor set layout.
         */
        vk::DescriptorSetLayout& GetSamplerLayout() { return mSamplerLayout; }

        /**
         * @brief Returns the sampler descriptor pool.
         */
        vk::DescriptorPool& GetSamplerDescriptorPool()
        {
            return mSamplerDescriptorPool;
        }

        /**
         * @brief Returns the fallback sampler set (set 1) with all 4 bindings
         * bound to the 1x1 white fallback texture.
         */
        vk::DescriptorSet& GetFallbackSamplerSet()
        {
            return mFallbackSamplerSet;
        }

        /**
         * @brief Returns the fallback image view for default material binding.
         */
        vk::ImageView& GetFallbackImageView() { return mFallbackImageView; }

        /**
         * @brief Returns the fallback sampler for default material binding.
         */
        vk::Sampler& GetFallbackSampler() { return mFallbackSampler; }

        /**
         * @brief Returns the black emissive fallback image view.
         */
        vk::ImageView& GetEmissiveFallbackImageView()
        {
            return mEmissiveFallbackImageView;
        }

        /**
         * @brief Returns the black emissive fallback sampler.
         */
        vk::Sampler& GetEmissiveFallbackSampler()
        {
            return mEmissiveFallbackSampler;
        }

        Ref<Device>       mDevice;
        Ref<FreyaOptions> mFreyaOptions;

        vk::RenderPass     mRenderPass;
        vk::PipelineLayout mPipelineLayout;
        vk::Pipeline       mGraphicsPipeline;
        Ref<Buffer>        mUniformBuffer;

        std::vector<vk::DescriptorSetLayout> mDescriptorSetLayouts;
        std::vector<vk::DescriptorSet>       mDescriptorSets;
        vk::DescriptorPool                   mDescriptorPool;

      private:
        vk::DescriptorSetLayout mSamplerLayout;
        vk::DescriptorPool      mSamplerDescriptorPool;

        // Fallback 1x1 white texture descriptor set (bound at set 1 in Begin())
        vk::DescriptorSet mFallbackSamplerSet;
        vk::Image         mFallbackImage;
        vk::DeviceMemory  mFallbackImageMemory;
        vk::ImageView     mFallbackImageView;
        vk::Sampler       mFallbackSampler;

        // Fallback 1x1 black emissive texture
        vk::Image        mEmissiveFallbackImage;
        vk::DeviceMemory mEmissiveFallbackMemory;
        vk::ImageView    mEmissiveFallbackImageView;
        vk::Sampler      mEmissiveFallbackSampler;
    };
} // namespace FREYA_NAMESPACE