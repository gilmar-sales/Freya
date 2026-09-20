#pragma once

#include "Freya/Asset/MaterialDescriptorResources.hpp"
#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/Core/UiDraw.hpp"
#include "Freya/FreyaOptions.hpp"

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Post-composite LOAD overlay for screen-space UI quads.
     *
     * Targets either the swapchain (PresentSrc) or an offscreen viewport
     * (ShaderReadOnly), matching DebugDrawPass. Bindless textures via
     * MaterialDescriptorResources (same heap as BillboardPass).
     */
    class UiPass
    {
      public:
        UiPass(const skr::Arc<Device>&                      device,
               const skr::Arc<FreyaOptions>&                freyaOptions,
               const skr::Arc<MaterialDescriptorResources>& materials,
               vk::RenderPass                               swapchainRenderPass,
               vk::RenderPass                               offscreenRenderPass,
               vk::PipelineLayout                           pipelineLayout,
               vk::DescriptorSetLayout                      setLayout,
               vk::DescriptorPool                           descriptorPool,
               const std::vector<vk::DescriptorSet>&        instanceSets,
               std::vector<skr::Arc<Buffer>>                instanceBuffers,
               vk::Pipeline                                 swapchainPipeline,
               vk::Pipeline                                 offscreenPipeline,
               std::vector<vk::Framebuffer>                 framebuffers,
               std::uint32_t                                maxQuads);

        ~UiPass();

        UiPass(const UiPass&)            = delete;
        UiPass& operator=(const UiPass&) = delete;

        void UpdateSwapchain(const skr::Arc<SwapChain>& swapChain);

        void UpdateOffscreen(const skr::Arc<Image>& color, vk::Extent2D extent);

        /**
         * @brief Snapshot @p source, upload SSBO, draw instanced quads.
         *
         * @param extent Framebuffer size used for NDC mapping (logical
         *               coords should already match this space, or be
         *               scaled by the caller).
         */
        void Draw(const skr::Arc<CommandPool>& commandPool,
                  const skr::Arc<SwapChain>&   swapChain, UiDraw& source,
                  vk::Extent2D extent, float logicalScale = 1.f) const;

      private:
        void destroyFramebuffers();

        skr::Arc<Device>                      mDevice;
        skr::Arc<FreyaOptions>                mFreyaOptions;
        skr::Arc<MaterialDescriptorResources> mMaterials;

        vk::RenderPass                 mSwapchainRenderPass {};
        vk::RenderPass                 mOffscreenRenderPass {};
        vk::PipelineLayout             mPipelineLayout {};
        vk::DescriptorSetLayout        mSetLayout {};
        vk::DescriptorPool             mDescriptorPool {};
        std::vector<vk::DescriptorSet> mInstanceSets;
        std::vector<skr::Arc<Buffer>>  mInstanceBuffers;
        vk::Pipeline                   mSwapchainPipeline {};
        vk::Pipeline                   mOffscreenPipeline {};

        std::vector<vk::Framebuffer> mFramebuffers;
        std::uint32_t                mMaxQuads = 0;
        vk::Extent2D                 mExtent {};
        bool                         mOffscreen = false;
    };

} // namespace FREYA_NAMESPACE
