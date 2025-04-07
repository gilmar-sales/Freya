#pragma once

#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/Instance.hpp"
#include "Freya/Core/Surface.hpp"

namespace FREYA_NAMESPACE
{
    struct SwapChainFrame
    {
        vk::Image       image;
        vk::ImageView   imageView;
        vk::Framebuffer frameBuffer;

        explicit operator bool() const
        {
            return image && imageView && frameBuffer;
        }
        static SwapChainFrame Null;
    };

    struct OffscreenBuffer
    {
        Ref<Image> depth;
        Ref<Image> sampler;
        Ref<Image> albedo;
        Ref<Image> normal;
        Ref<Image> position;
    };

    class SwapChain
    {
      public:
        SwapChain(const Ref<Device>&                  device,
                  const Ref<Instance>&                instance,
                  const Ref<Surface>&                 surface,
                  const vk::SwapchainKHR              swapChain,
                  const std::vector<SwapChainFrame>&  frames,
                  const std::vector<OffscreenBuffer>& offscreenBuffers,
                  const std::vector<vk::Semaphore>&   imageAvailableSemaphores,
                  const std::vector<vk::Semaphore>&   renderFinishedSemaphores,
                  const std::vector<vk::Fence>&       inFlightFences) :
            mDevice(device), mInstance(instance), mSurface(surface),
            mSwapChain(swapChain), mFrames(frames), mCurrentFrameIndex(0),
            mOffscreenBuffers(offscreenBuffers),
            mImageAvailableSemaphores(imageAvailableSemaphores),
            mRenderFinishedSemaphores(renderFinishedSemaphores),
            mInFlightFences(inFlightFences)
        {
            mExtent = surface->QueryExtent();
        }

        ~SwapChain();

        [[nodiscard]] const vk::SwapchainKHR& Get() const { return mSwapChain; }

        Ref<Surface>                       GetSurface() { return mSurface; }
        const std::vector<SwapChainFrame>& GetFrames() { return mFrames; }
        const size_t GetFrameCount() { return mFrames.size(); }

        std::vector<OffscreenBuffer>& GetOffscreenBuffers()
        {
            return mOffscreenBuffers;
        }

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

        std::vector<OffscreenBuffer> mOffscreenBuffers;
    };

} // namespace FREYA_NAMESPACE
