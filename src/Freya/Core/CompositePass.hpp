#pragma once

#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/FreyaOptions.hpp"

#include <vulkan/vulkan.hpp>

namespace FREYA_NAMESPACE
{
    class CompositePass
    {
      public:
        CompositePass(const skr::Arc<Device>&               device,
                      const skr::Arc<FreyaOptions>&         freyaOptions,
                      const skr::Arc<Surface>&              surface,
                      vk::RenderPass                        renderPass,
                      vk::PipelineLayout                    pipelineLayout,
                      vk::Pipeline                          compositePipeline,
                      const std::vector<vk::Framebuffer>&   framebuffers,
                      vk::DescriptorPool                    descriptorPool,
                      const vk::DescriptorSetLayout         descriptorSetLayout,
                      const std::vector<vk::DescriptorSet>& descriptorSets);

        ~CompositePass();

        vk::RenderPass& GetRenderPass() { return mRenderPass; }

        vk::Pipeline& GetPipeline() { return mCompositePipeline; }

        std::size_t GetFramebufferCount() const { return mFramebuffers.size(); }
        vk::Framebuffer& GetFramebuffer(std::size_t index)
        {
            return mFramebuffers[index];
        }

        void Begin(const skr::Arc<SwapChain>    swapChain,
                   const skr::Arc<CommandPool>& commandPool,
                   const vk::ClearValue&        clearColor) const;

        /**
         * @brief Begin composite into a custom framebuffer (e.g. RenderTarget).
         *
         * @param renderPass Compatible with this pass's pipeline render pass
         * @param framebuffer Destination framebuffer
         * @param extent      Render area extent
         */
        void Begin(vk::RenderPass               renderPass,
                   vk::Framebuffer              framebuffer,
                   vk::Extent2D                 extent,
                   const skr::Arc<CommandPool>& commandPool,
                   const vk::ClearValue&        clearColor) const;

        void BindPipeline(const skr::Arc<CommandPool>& commandPool,
                          std::uint32_t                frameIndex) const;

        /**
         * @param tonemapHdr 1.0 applies ACES+gamma (deferred HDR Scene Color);
         *                   0.0 leaves LDR inputs untouched (forward).
         */
        void DrawFullscreenTriangle(const skr::Arc<CommandPool>& commandPool,
                                    float tonemapHdr = 0.0f) const;

        void End(const skr::Arc<CommandPool> commandPool) const;

        void UpdateDescriptorSet(std::uint32_t          frameIndex,
                                 const skr::Arc<Image>& opaqueImage,
                                 const skr::Arc<Image>& translucentImage,
                                 const skr::Arc<Image>& bloomResultImage,
                                 vk::Sampler            sampler);

      private:
        struct BoundImages
        {
            vk::ImageView opaque {};
            vk::ImageView translucent {};
            vk::ImageView bloom {};
            vk::Sampler   sampler {};
        };

        skr::Arc<Device>       mDevice;
        skr::Arc<FreyaOptions> mFreyaOptions;
        skr::Arc<Surface>      mSurface;

        vk::RenderPass     mRenderPass;
        vk::PipelineLayout mPipelineLayout;
        vk::Pipeline       mCompositePipeline;

        std::vector<vk::Framebuffer> mFramebuffers;

        vk::DescriptorPool             mDescriptorPool;
        vk::DescriptorSetLayout        mDescriptorSetLayout;
        std::vector<vk::DescriptorSet> mDescriptorSets;
        std::vector<BoundImages>       mBoundImages;
    };
} // namespace FREYA_NAMESPACE
