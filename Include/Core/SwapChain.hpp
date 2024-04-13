#pragma once

#include "Core/Device.hpp"
#include "Core/Image.hpp"
#include "Core/Instance.hpp"

namespace FREYA_NAMESPACE
{
    struct SwapChainFrame
    {
        vk::Image       image;
        vk::ImageView   imageView;
        vk::Framebuffer frameBuffer;

        operator bool() const { return image && imageView && frameBuffer; }
        static SwapChainFrame Null;
    };

    class SwapChain
    {
      public:
        SwapChain(std::shared_ptr<Device> device,
                  std::shared_ptr<Instance>
                      instance,
                  std::shared_ptr<Surface>
                                   surface,
                  vk::SwapchainKHR swapChain,
                  std::vector<SwapChainFrame>
                      frames,
                  std::shared_ptr<Image>
                      depthImage,
                  std::shared_ptr<Image>
                      sampleImage) :
            mDevice(device),
            mInstance(instance), mSurface(surface),
            mSwapChain(swapChain), mFrames(frames), mCurrentFrameIndex(0),
            mDepthImage(depthImage), mSampleImage(sampleImage)
        {
            mExtent = surface->QueryExtent();
        }

        ~SwapChain();

        const vk::SwapchainKHR&            Get() { return mSwapChain; }
        const std::shared_ptr<Surface>&    GetSurface() { return mSurface; }
        const std::vector<SwapChainFrame>& GetFrames() { return mFrames; }

        const SwapChainFrame& GetNextFrame(vk::Semaphore semaphore);
        std::uint32_t         GetCurrentFrameIndex() { return mCurrentFrameIndex; }
        const vk::Extent2D&   GetExtent() { return mExtent; }

      private:
        std::shared_ptr<Device>   mDevice;
        std::shared_ptr<Instance> mInstance;
        std::shared_ptr<Surface>  mSurface;

        vk::SwapchainKHR            mSwapChain;
        vk::Extent2D                mExtent;
        std::vector<SwapChainFrame> mFrames;
        std::uint32_t               mCurrentFrameIndex;

        std::shared_ptr<Image> mDepthImage;
        std::shared_ptr<Image> mSampleImage;
    };

} // namespace FREYA_NAMESPACE
