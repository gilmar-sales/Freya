#pragma once

#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/UniformBuffer.hpp"
#include "Freya/FreyaOptions.hpp"

#include <functional>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Clearest "no entity" value written to the ID buffer.
     *
     * Matches SelectionContext::Invalid / missing Freyr entities (0 is valid).
     */
    constexpr std::uint32_t kPickMissId = 0xFFFFFFFFu;

    /**
     * @brief Offscreen integer ID pass for GPU mouse picking.
     *
     * Owns an R32_UINT color target, depth target, pick pipeline (projection
     * UBO + entity push constant), and a staging buffer for 1-pixel readback.
     */
    class PickPass
    {
      public:
        PickPass(const skr::Arc<Device>&         device,
                 const skr::Arc<PhysicalDevice>& physicalDevice,
                 const skr::Arc<FreyaOptions>&   freyaOptions,
                 vk::RenderPass                  renderPass,
                 vk::PipelineLayout              pipelineLayout,
                 vk::Pipeline                    pipeline,
                 vk::DescriptorSetLayout         descriptorSetLayout,
                 vk::DescriptorPool              descriptorPool,
                 vk::DescriptorSet               descriptorSet,
                 const skr::Arc<Buffer>&         uniformBuffer,
                 const skr::Arc<Buffer>&         stagingBuffer,
                 const skr::Arc<Image>&          colorImage,
                 const skr::Arc<Image>&          depthImage,
                 vk::Framebuffer                 framebuffer,
                 vk::Extent2D                    extent);

        ~PickPass();

        PickPass(const PickPass&)            = delete;
        PickPass& operator=(const PickPass&) = delete;

        /**
         * @brief Swaps in resized color/depth images and rebuilds the
         * framebuffer.
         */
        void Resize(vk::Extent2D         extent,
                    const skr::Arc<Image>& colorImage,
                    const skr::Arc<Image>& depthImage);

        /**
         * @brief Renders entity IDs into the pick buffer.
         *
         * Clears to kPickMissId, binds the pick pipeline and projection UBO,
         * then invokes drawScene. drawScene should call PushEntityId per mesh
         * and issue draws with no material binding.
         */
        void Render(const skr::Arc<CommandPool>&   commandPool,
                    const ProjectionUniformBuffer& projection,
                    const std::function<void()>&   drawScene);

        /**
         * @brief Pushes the per-draw entity ID push constant.
         */
        void PushEntityId(const skr::Arc<CommandPool>& commandPool,
                          std::uint32_t                entityId) const;

        /**
         * @brief Copies one texel from the ID image into the staging buffer.
         */
        void CopyPixel(const skr::Arc<CommandPool>& commandPool,
                       std::uint32_t                x,
                       std::uint32_t                y);

        /**
         * @brief Maps the staging buffer and returns the last copied ID.
         * @note Caller must ensure the GPU has finished the copy.
         */
        [[nodiscard]] std::uint32_t ReadPixel() const;

        [[nodiscard]] vk::Extent2D GetExtent() const { return mExtent; }

        [[nodiscard]] vk::Pipeline GetPipeline() const { return mPipeline; }

        [[nodiscard]] vk::PipelineLayout GetPipelineLayout() const
        {
            return mPipelineLayout;
        }

      private:
        void destroyFramebufferResources();
        void createFramebuffer();

        skr::Arc<Device>         mDevice;
        skr::Arc<PhysicalDevice> mPhysicalDevice;
        skr::Arc<FreyaOptions>   mFreyaOptions;

        vk::RenderPass          mRenderPass;
        vk::PipelineLayout      mPipelineLayout;
        vk::Pipeline            mPipeline;
        vk::DescriptorSetLayout mDescriptorSetLayout;
        vk::DescriptorPool      mDescriptorPool;
        vk::DescriptorSet       mDescriptorSet;

        skr::Arc<Buffer> mUniformBuffer;
        skr::Arc<Buffer> mStagingBuffer;
        skr::Arc<Image>  mColorImage;
        skr::Arc<Image>  mDepthImage;
        vk::Framebuffer  mFramebuffer = VK_NULL_HANDLE;
        vk::Extent2D     mExtent {};
    };

} // namespace FREYA_NAMESPACE
