#pragma once

#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/DebugDraw.hpp"
#include "Freya/Core/Device.hpp"
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
     * @brief Post-composite swapchain LOAD overlay for colored line lists.
     */
    class DebugDrawPass
    {
      public:
        DebugDrawPass(const skr::Arc<Device>&       device,
                      const skr::Arc<FreyaOptions>& freyaOptions,
                      vk::RenderPass                renderPass,
                      vk::PipelineLayout            pipelineLayout,
                      vk::Pipeline                  pipeline,
                      std::vector<vk::Framebuffer>
                          framebuffers,
                      std::vector<skr::Arc<Buffer>>
                                    vertexBuffers,
                      std::uint32_t maxVertices);

        ~DebugDrawPass();

        DebugDrawPass(const DebugDrawPass&)            = delete;
        DebugDrawPass& operator=(const DebugDrawPass&) = delete;

        /**
         * @brief Upload lines and draw into the current swapchain image.
         *
         * No-op when `verts` is empty. Skips when swapchain image count
         * mismatch.
         */
        void Draw(const skr::Arc<SwapChain>&   swapChain,
                  const skr::Arc<CommandPool>& commandPool,
                  std::span<const DebugDrawVertex>
                                   verts,
                  const glm::mat4& viewProj) const;

      private:
        skr::Arc<Device>       mDevice;
        skr::Arc<FreyaOptions> mFreyaOptions;

        vk::RenderPass                mRenderPass;
        vk::PipelineLayout            mPipelineLayout;
        vk::Pipeline                  mPipeline;
        std::vector<vk::Framebuffer>  mFramebuffers;
        std::vector<skr::Arc<Buffer>> mVertexBuffers;
        std::uint32_t                 mMaxVertices = 0;
    };

} // namespace FREYA_NAMESPACE
