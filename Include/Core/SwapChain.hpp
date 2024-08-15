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

        explicit              operator bool() const { return image && imageView && frameBuffer; }
        static SwapChainFrame Null;
    };

    class SwapChain
    {
      public:
        SwapChain(const Ref<Device>&                 device,
                  const Ref<Instance>&               instance,
                  const Ref<Surface>&                surface,
                  const vk::SwapchainKHR             swapChain,
                  const std::vector<SwapChainFrame>& frames,
                  const Ref<Image>&                  depthImage,
                  const Ref<Image>&                  sampleImage) :
            mDevice(device),
            mInstance(instance), mSurface(surface),
            mSwapChain(swapChain), mFrames(frames), mCurrentFrameIndex(0),
            mDepthImage(depthImage), mSampleImage(sampleImage)
        {
            mExtent = surface->QueryExtent();
        }

        ~SwapChain();

        [[nodiscard]] const vk::SwapchainKHR& Get() const { return mSwapChain; }
        Ref<Surface>                          GetSurface() { return mSurface; }
        const std::vector<SwapChainFrame>&    GetFrames() { return mFrames; }

        const SwapChainFrame&       GetNextFrame(vk::Semaphore semaphore);
        [[nodiscard]] std::uint32_t GetCurrentFrameIndex() const { return mCurrentFrameIndex; }
        [[nodiscard]] vk::Extent2D  GetExtent() const { return mExtent; }

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
