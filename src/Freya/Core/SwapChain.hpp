#pragma once

#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/Instance.hpp"
#include "Freya/Core/Surface.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Frame data structure containing swap chain image resources.
     */
    struct SwapChainFrame
    {
        vk::Image       image;       ///< Swap chain image
        vk::ImageView   imageView;   ///< Image view for rendering
        vk::Framebuffer frameBuffer; ///< Framebuffer for this frame

        /**
         * @brief Conversion operator to check if all resources are valid.
         * @return true if image, imageView, and frameBuffer are all non-null
         */
        explicit operator bool() const
        {
            return image && imageView && frameBuffer;
        }
        static SwapChainFrame Null; ///< Null frame sentinel
    };

    /**
     * @brief Manages Vulkan swapchain, frames, and frame synchronization.
     *
     * Handles swapchain creation, image acquisition, frame synchronization
     * with semaphores and fences, and presentation. Manages depth and sample
     * images for MSAA.
     *
     * @param device                    Device reference
     * @param instance                  Instance reference
     * @param surface                   Surface reference
     * @param swapChain                Vulkan swapchain handle
     * @param frames                    Vector of SwapChainFrame objects
     * @param depthImage                Depth attachment image
     * @param sampleImage               MSAA sample image
     * @param imageAvailableSemaphores  Semaphores for image acquisition
     * @param renderFinishedSemaphores  Semaphores for render completion
     * @param inFlightFences            Fences for frame synchronization
     */
    class SwapChain
    {
      public:
        SwapChain(const Ref<Device>&                 device,
                  const Ref<Instance>&               instance,
                  const Ref<Surface>&                surface,
                  const vk::SwapchainKHR             swapChain,
                  const std::vector<SwapChainFrame>& frames,
                  const Ref<Image>&                  depthImage,
                  const Ref<Image>&                  sampleImage,
                  const std::vector<vk::Semaphore>&  imageAvailableSemaphores,
                  const std::vector<vk::Semaphore>&  renderFinishedSemaphores,
                  const std::vector<vk::Fence>&      inFlightFences) :
            mDevice(device), mInstance(instance), mSurface(surface),
            mSwapChain(swapChain), mFrames(frames), mCurrentFrameIndex(0),
            mDepthImage(depthImage), mSampleImage(sampleImage),
            mImageAvailableSemaphores(imageAvailableSemaphores),
            mRenderFinishedSemaphores(renderFinishedSemaphores),
            mInFlightFences(inFlightFences)
        {
            mExtent = surface->QueryExtent();
        }

        ~SwapChain();

        /**
         * @brief Returns the underlying swapchain handle.
         */
        [[nodiscard]] const vk::SwapchainKHR& Get() const { return mSwapChain; }

        /**
         * @brief Returns the surface associated with this swapchain.
         */
        Ref<Surface> GetSurface() { return mSurface; }

        /**
         * @brief Returns all swapchain frames.
         */
        const std::vector<SwapChainFrame>& GetFrames() { return mFrames; }

        /**
         * @brief Returns the number of frames in the swapchain.
         */
        const size_t GetFrameCount() { return mFrames.size(); }

        /**
         * @brief Waits for the next frame's fence to be signaled.
         * @throws std::runtime_error if wait fails
         */
        void WaitNextFrame();

        /**
         * @brief Resets the current frame's fence.
         * @throws std::runtime_error if reset fails
         */
        void BeginNextFrame();

        /**
         * @brief Acquires the next swapchain image.
         * @return SwapChainFrame for the acquired image
         * @throws std::runtime_error if acquisition fails
         */
        const SwapChainFrame& GetNextFrame();

        /**
         * @brief Returns the current frame without acquiring a new one.
         */
        const SwapChainFrame& GetCurrentFrame() const
        {
            return mFrames[mCurrentFrameIndex];
        }

        /**
         * @brief Waits for command buffers to finish execution.
         * @param commandBuffers Command buffers to wait on
         */
        void WaitCommandBuffersSubmission(
            std::vector<vk::CommandBuffer> commandBuffers);

        /**
         * @brief Submits command buffers and presents the current image.
         * @param commandBuffers Command buffers to submit
         * @return vk::Result of the presentation
         * @throws std::runtime_error if submission fails
         */
        vk::Result Present(std::vector<vk::CommandBuffer> commandBuffers);

        /**
         * @brief Returns the current frame index (ring buffer position).
         */
        [[nodiscard]] std::uint32_t GetCurrentFrameIndex() const
        {
            return mCurrentFrameIndex;
        }

        /**
         * @brief Returns the swapchain extent (width/height).
         */
        [[nodiscard]] vk::Extent2D GetExtent() const { return mExtent; }

      private:
        Ref<Device>   mDevice;
        Ref<Instance> mInstance;
        Ref<Surface>  mSurface;

        vk::SwapchainKHR            mSwapChain;
        vk::Extent2D                mExtent;
        std::vector<SwapChainFrame> mFrames;

        std::vector<vk::Semaphore> mImageAvailableSemaphores;
        std::vector<vk::Semaphore> mRenderFinishedSemaphores;
        std::vector<vk::Fence>     mInFlightFences;

        std::uint32_t mCurrentFrameIndex;
        std::uint32_t mCurrentImageIndex;

        Ref<Image> mDepthImage;
        Ref<Image> mSampleImage;
    };

} // namespace FREYA_NAMESPACE
