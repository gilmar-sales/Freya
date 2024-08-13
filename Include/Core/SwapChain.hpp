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
        SwapChain(Ref<Device> device,
                  Ref<Instance>
                      instance,
                  Ref<Surface>
                                   surface,
                  vk::SwapchainKHR swapChain,
                  std::vector<SwapChainFrame>
                      frames,
                  Ref<Image>
                      depthImage,
                  Ref<Image>
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
        const Ref<Surface>&    GetSurface() { return mSurface; }
        const std::vector<SwapChainFrame>& GetFrames() { return mFrames; }

        const SwapChainFrame& GetNextFrame(vk::Semaphore semaphore);
        std::uint32_t         GetCurrentFrameIndex() { return mCurrentFrameIndex; }
        const vk::Extent2D&   GetExtent() { return mExtent; }

      private:
        Ref<Device>   mDevice;
        Ref<Instance> mInstance;
        Ref<Surface>  mSurface;

        vk::SwapchainKHR            mSwapChain;
        vk::Extent2D                mExtent;
        std::vector<SwapChainFrame> mFrames;
        std::uint32_t               mCurrentFrameIndex;

        Ref<Image> mDepthImage;
        Ref<Image> mSampleImage;
    };

} // namespace FREYA_NAMESPACE
