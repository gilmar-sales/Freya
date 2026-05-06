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
        CompositePass(const Ref<Device>&                    device,
                      const Ref<FreyaOptions>&              freyaOptions,
                      const Ref<Surface>&                   surface,
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

        void Begin(const Ref<SwapChain>    swapChain,
                   const Ref<CommandPool>& commandPool,
                   const vk::ClearValue&   clearColor) const;

        void BindPipeline(const Ref<CommandPool>& commandPool,
                          std::uint32_t           frameIndex) const;

        void DrawFullscreenTriangle(const Ref<CommandPool>& commandPool) const;

        void End(const Ref<CommandPool> commandPool) const;

        void UpdateDescriptorSet(std::uint32_t     frameIndex,
                                 const Ref<Image>& opaqueImage,
                                 const Ref<Image>& translucentImage,
                                 const Ref<Image>& bloomResultImage,
                                 vk::Sampler       sampler) const;

      private:
        Ref<Device>       mDevice;
        Ref<FreyaOptions> mFreyaOptions;
        Ref<Surface>      mSurface;

        vk::RenderPass     mRenderPass;
        vk::PipelineLayout mPipelineLayout;
        vk::Pipeline       mCompositePipeline;

        std::vector<vk::Framebuffer> mFramebuffers;

        vk::DescriptorPool             mDescriptorPool;
        vk::DescriptorSetLayout        mDescriptorSetLayout;
        std::vector<vk::DescriptorSet> mDescriptorSets;
    };
} // namespace FREYA_NAMESPACE
