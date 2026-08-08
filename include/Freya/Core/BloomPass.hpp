#pragma once

#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/FreyaOptions.hpp"

#include <vulkan/vulkan.hpp>

namespace FREYA_NAMESPACE
{
    enum : std::uint32_t
    {
        BloomThresholdSubpass,
        BloomDownsampleSubpass,
        BloomUpsampleSubpass
    };

    enum : std::uint32_t
    {
        BloomThresholdAttachment,
        BloomDownAttachment,
        BloomUpAttachment
    };

    class BloomPass
    {
      public:
        BloomPass(
            const skr::Arc<Device>&                     device,
            const skr::Arc<FreyaOptions>&               freyaOptions,
            const skr::Arc<Surface>&                    surface,
            vk::Extent2D                                halfExtent,
            vk::RenderPass                              renderPass,
            vk::PipelineLayout                          pipelineLayout,
            vk::Pipeline                                thresholdPipeline,
            vk::Pipeline                                downsamplePipeline,
            vk::Pipeline                                upsamplePipeline,
            const skr::Arc<Image>&                      bloomThresholdImage,
            const skr::Arc<Image>&                      bloomDownImage,
            const skr::Arc<Image>&                      bloomUpImage,
            const std::vector<vk::Framebuffer>&         framebuffers,
            vk::DescriptorPool                          descriptorPool,
            const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
            const std::vector<vk::DescriptorSet>&       descriptorSets);

        ~BloomPass();

        vk::RenderPass& GetRenderPass() { return mRenderPass; }

        vk::Pipeline& GetPipeline(std::uint32_t subpass);

        skr::Arc<Image> GetBloomUpImage() const { return mBloomUpImage; }

        std::size_t GetFramebufferCount() const { return mFramebuffers.size(); }
        vk::Framebuffer& GetFramebuffer(std::size_t index)
        {
            return mFramebuffers[index];
        }

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

      private:
        skr::Arc<Device>       mDevice;
        skr::Arc<FreyaOptions> mFreyaOptions;
        skr::Arc<Surface>      mSurface;

        vk::Extent2D mHalfExtent; ///< Half-resolution extent for bloom

        vk::RenderPass     mRenderPass;
        vk::PipelineLayout mPipelineLayout;

        std::array<vk::Pipeline, 3> mPipelines;

        skr::Arc<Image> mBloomThresholdImage;
        skr::Arc<Image> mBloomDownImage;
        skr::Arc<Image> mBloomUpImage;

        std::vector<vk::Framebuffer> mFramebuffers;

        vk::DescriptorPool                   mDescriptorPool;
        std::vector<vk::DescriptorSetLayout> mDescriptorSetLayouts;
        std::vector<vk::DescriptorSet>       mDescriptorSets;

        mutable bool mLabelActive = false;

        static const char* GetSubpassLabel(std::uint32_t subpass);
        static DebugRegion GetSubpassRegion(std::uint32_t subpass);
    };
} // namespace FREYA_NAMESPACE
