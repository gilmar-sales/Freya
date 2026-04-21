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
            const vk::DescriptorPool                    samplerDescriptorPool) :
            mDevice(device), mFreyaOptions(freyaOptions),
            mRenderPass(renderPass), mPipelineLayout(pipelineLayout),
            mGraphicsPipeline(graphicsPipeline), mUniformBuffer(uniformBuffer),
            mDescriptorSetLayouts(descriptorSetLayouts),
            mDescriptorSets(descriptorSets), mDescriptorPool(descriptorPool),
            mSamplerLayout(samplerLayout),
            mSamplerDescriptorPool(samplerDescriptorPool)
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
    };
} // namespace FREYA_NAMESPACE