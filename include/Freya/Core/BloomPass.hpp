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
            std::vector<skr::Arc<Image>>                bloomThresholdImages,
            std::vector<skr::Arc<Image>>                bloomDownImages,
            std::vector<skr::Arc<Image>>                bloomUpImages,
            const std::vector<vk::Framebuffer>&         framebuffers,
            vk::DescriptorPool                          descriptorPool,
            const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
            const std::vector<vk::DescriptorSet>&       descriptorSets,
            vk::Sampler                                 sampler);

        ~BloomPass();

        vk::RenderPass& GetRenderPass() { return mRenderPass; }

        vk::Pipeline& GetPipeline(std::uint32_t subpass);

        [[nodiscard]] skr::Arc<Image> GetBloomUpImage(
            std::uint32_t frameIndex) const;

        std::size_t GetFramebufferCount() const { return mFramebuffers.size(); }
        vk::Framebuffer& GetFramebuffer(std::size_t index)
        {
            return mFramebuffers[index];
        }

        void Begin(const skr::Arc<CommandPool>& commandPool,
                   std::uint32_t                frameIndex) const;

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

        /**
         * @brief Point this frame's threshold pass at a HDR source image.
         *
         * Call only while the corresponding in-flight fence has been waited
         * (e.g. from Rebuild after device idle, or after WaitNextFrame).
         */
        void SetThresholdInput(std::uint32_t          frameIndex,
                               const skr::Arc<Image>& sourceImage);

      private:
        skr::Arc<Device>       mDevice;
        skr::Arc<FreyaOptions> mFreyaOptions;
        skr::Arc<Surface>      mSurface;

        vk::Extent2D mHalfExtent; ///< Half-resolution extent for bloom

        vk::RenderPass     mRenderPass;
        vk::PipelineLayout mPipelineLayout;

        std::array<vk::Pipeline, 3> mPipelines;

        std::vector<skr::Arc<Image>> mBloomThresholdImages;
        std::vector<skr::Arc<Image>> mBloomDownImages;
        std::vector<skr::Arc<Image>> mBloomUpImages;

        std::vector<vk::Framebuffer> mFramebuffers;

        vk::DescriptorPool                   mDescriptorPool;
        std::vector<vk::DescriptorSetLayout> mDescriptorSetLayouts;
        std::vector<vk::DescriptorSet>       mDescriptorSets;
        vk::Sampler                          mSampler {};

        mutable bool mLabelActive = false;

        static const char* GetSubpassLabel(std::uint32_t subpass);
        static DebugRegion GetSubpassRegion(std::uint32_t subpass);
    };
} // namespace FREYA_NAMESPACE
