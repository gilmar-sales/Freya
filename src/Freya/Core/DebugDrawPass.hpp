#pragma once

#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/DebugDraw.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/FreyaOptions.hpp"

#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Post-composite LOAD overlay for colored line lists.
     *
     * Targets either the swapchain (PresentSrc) or an offscreen viewport
     * (ShaderReadOnly) so debug lines appear in ImGui SetViewportTarget paths.
     */
    class DebugDrawPass
    {
      public:
        DebugDrawPass(const skr::Arc<Device>&       device,
                      const skr::Arc<FreyaOptions>& freyaOptions,
                      vk::RenderPass                swapchainRenderPass,
                      vk::RenderPass                offscreenRenderPass,
                      vk::PipelineLayout            pipelineLayout,
                      vk::Pipeline                  swapchainPipeline,
                      vk::Pipeline                  offscreenPipeline,
                      std::vector<vk::Framebuffer>
                          framebuffers,
                      std::vector<skr::Arc<Buffer>>
                                    vertexBuffers,
                      std::uint32_t maxVertices);

        ~DebugDrawPass();

        DebugDrawPass(const DebugDrawPass&)            = delete;
        DebugDrawPass& operator=(const DebugDrawPass&) = delete;

        void UpdateSwapchain(const skr::Arc<SwapChain>& swapChain);

        void UpdateOffscreen(const skr::Arc<Image>& color, vk::Extent2D extent);

        /**
         * @brief Upload lines and draw into the active target.
         */
        void Draw(const skr::Arc<SwapChain>&   swapChain,
                  const skr::Arc<CommandPool>& commandPool,
                  std::span<const DebugDrawVertex>
                                   verts,
                  const glm::mat4& viewProj) const;

      private:
        void destroyFramebuffers();

        skr::Arc<Device>       mDevice;
        skr::Arc<FreyaOptions> mFreyaOptions;

        vk::RenderPass     mSwapchainRenderPass {};
        vk::RenderPass     mOffscreenRenderPass {};
        vk::PipelineLayout mPipelineLayout {};
        vk::Pipeline       mSwapchainPipeline {};
        vk::Pipeline       mOffscreenPipeline {};

        std::vector<vk::Framebuffer>  mFramebuffers;
        std::vector<skr::Arc<Buffer>> mVertexBuffers;
        std::uint32_t                 mMaxVertices = 0;
        vk::Extent2D                  mExtent {};
        bool                          mOffscreen = false;
    };

} // namespace FREYA_NAMESPACE
