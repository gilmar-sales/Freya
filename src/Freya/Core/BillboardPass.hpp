#pragma once

#include "Freya/Asset/MaterialDescriptorResources.hpp"
#include "Freya/Core/BillboardDraw.hpp"
#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/FreyaOptions.hpp"

#include <span>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    enum class BillboardTarget : std::uint32_t
    {
        Hdr = 0,
        Ldr = 1,
    };

    /**
     * @brief Instanced camera-facing quads (VFX HDR or world-space UI LDR).
     */
    class BillboardPass
    {
      public:
        struct Pipelines
        {
            vk::Pipeline alphaDepth {};
            vk::Pipeline alphaNoDepth {};
            vk::Pipeline addDepth {};
            vk::Pipeline addNoDepth {};
        };

        BillboardPass(
            const skr::Arc<Device>&                      device,
            const skr::Arc<FreyaOptions>&                freyaOptions,
            const skr::Arc<MaterialDescriptorResources>& materials,
            vk::RenderPass hdrRenderPass, vk::RenderPass ldrRenderPass,
            vk::PipelineLayout                    pipelineLayout,
            vk::DescriptorSetLayout               setLayout,
            vk::DescriptorPool                    descriptorPool,
            const std::vector<vk::DescriptorSet>& instanceSets,
            std::vector<skr::Arc<Buffer>>         instanceBuffers,
            Pipelines hdrPipelines, Pipelines ldrPipelines,
            std::vector<vk::Framebuffer> ldrFramebuffers, vk::Extent2D extent,
            std::uint32_t maxQuads);

        ~BillboardPass();

        BillboardPass(const BillboardPass&)            = delete;
        BillboardPass& operator=(const BillboardPass&) = delete;

        void UpdateHdrTargets(std::span<const skr::Arc<Image>> colors,
                              const skr::Arc<Image>&           depth,
                              vk::Extent2D                     extent);

        void UpdateLdrDepth(const skr::Arc<Image>&     depth,
                            const skr::Arc<SwapChain>& swapChain);

        void Draw(const skr::Arc<CommandPool>& commandPool,
                  const skr::Arc<SwapChain>& swapChain, BillboardTarget target,
                  BillboardLayer layer, const BillboardDraw& source,
                  const glm::mat4& view, const glm::mat4& proj) const;

      private:
        void destroyHdrFramebuffers();
        void destroyLdrFramebuffers();

        [[nodiscard]] vk::Pipeline pickPipeline(
            BillboardTarget target, BillboardBlend blend, bool depthTest) const;

        skr::Arc<Device>                      mDevice;
        skr::Arc<FreyaOptions>                mFreyaOptions;
        skr::Arc<MaterialDescriptorResources> mMaterials;

        vk::RenderPass                 mHdrRenderPass {};
        vk::RenderPass                 mLdrRenderPass {};
        vk::PipelineLayout             mPipelineLayout {};
        vk::DescriptorSetLayout        mSetLayout {};
        vk::DescriptorPool             mDescriptorPool {};
        std::vector<vk::DescriptorSet> mInstanceSets;
        std::vector<skr::Arc<Buffer>>  mInstanceBuffers;

        Pipelines mHdrPipelines {};
        Pipelines mLdrPipelines {};

        std::vector<vk::Framebuffer> mHdrFramebuffers;
        std::vector<vk::ImageView>   mHdrColorViews;
        vk::ImageView                mHdrDepthView {};

        std::vector<vk::Framebuffer> mLdrFramebuffers;
        vk::ImageView                mLdrDepthView {};

        vk::Extent2D  mHdrExtent {};
        vk::Extent2D  mLdrExtent {};
        std::uint32_t mMaxQuads = 0;
    };

} // namespace FREYA_NAMESPACE
