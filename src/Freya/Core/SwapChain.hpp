#pragma once

#include "Freya/Core/Device.hpp"
#include "Freya/Core/Instance.hpp"
#include "Freya/Core/Surface.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Frame data structure containing swap chain image resources.
     */
    struct SwapChainFrame
    {
        vk::Image     image;     ///< Swap chain image
        vk::ImageView imageView; ///< Image view for rendering

        explicit operator bool() const { return image && imageView; }
        static SwapChainFrame Null;
    };

    /**
     * @brief Manages Vulkan swapchain, frames, and frame synchronization.
     */
    class SwapChain
    {
      public:
        SwapChain(const skr::Arc<Device>&            device,
                  const skr::Arc<Instance>&          instance,
                  const skr::Arc<Surface>&           surface,
                  const vk::SwapchainKHR             swapChain,
                  const std::vector<SwapChainFrame>& frames,
                  const std::vector<vk::Semaphore>&  imageAvailableSemaphores,
                  const std::vector<vk::Semaphore>&  renderFinishedSemaphores,
                  const std::vector<vk::Fence>&      inFlightFences) :
            mDevice(device), mInstance(instance), mSurface(surface),
            mSwapChain(swapChain), mFrames(frames), mCurrentFrameIndex(0),
            mImageAvailableSemaphores(imageAvailableSemaphores),
            mRenderFinishedSemaphores(renderFinishedSemaphores),
            mInFlightFences(inFlightFences)
        {
            mExtent = surface->QueryExtent();
        }

        ~SwapChain();

        [[nodiscard]] const vk::SwapchainKHR& Get() const { return mSwapChain; }

        skr::Arc<Surface> GetSurface() { return mSurface; }

        const std::vector<SwapChainFrame>& GetFrames() { return mFrames; }

        const size_t GetFrameCount() { return mFrames.size(); }

        void WaitNextFrame();

        void BeginNextFrame();

        const SwapChainFrame& GetNextFrame();

        const SwapChainFrame& GetCurrentFrame() const
        {
            return mFrames[mCurrentFrameIndex];
        }

        void WaitCommandBuffersSubmission(
            std::vector<vk::CommandBuffer> commandBuffers);

        vk::Result Present(std::vector<vk::CommandBuffer> commandBuffers);

        [[nodiscard]] std::uint32_t GetCurrentFrameIndex() const
        {
            return mCurrentFrameIndex;
        }

        [[nodiscard]] std::uint32_t GetCurrentImageIndex() const
        {
            return mCurrentImageIndex;
        }

        [[nodiscard]] vk::Extent2D GetExtent() const { return mExtent; }

      private:
        skr::Arc<Device>   mDevice;
        skr::Arc<Instance> mInstance;
        skr::Arc<Surface>  mSurface;

        vk::SwapchainKHR            mSwapChain;
        vk::Extent2D                mExtent;
        std::vector<SwapChainFrame> mFrames;

        std::vector<vk::Semaphore> mImageAvailableSemaphores;
        std::vector<vk::Semaphore> mRenderFinishedSemaphores;
        std::vector<vk::Fence>     mInFlightFences;

        std::uint32_t mCurrentFrameIndex;
        std::uint32_t mCurrentImageIndex;
    };

} // namespace FREYA_NAMESPACE
