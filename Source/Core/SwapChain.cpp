#include "Core/SwapChain.hpp"

#include "Core/Device.hpp"

namespace FREYA_NAMESPACE
{
    SwapChainFrame SwapChainFrame::Null = SwapChainFrame {};

    SwapChain::~SwapChain()
    {
        mDepthImage.reset();
        mSampleImage.reset();

        for (const auto& frame : mFrames)
        {
            mDevice->Get().destroyFramebuffer(frame.frameBuffer);
            mDevice->Get().destroyImageView(frame.imageView);
        }

        mDevice->Get().destroySwapchainKHR(mSwapChain);
    }

    const SwapChainFrame& SwapChain::GetNextFrame(vk::Semaphore semaphore)
    {
        const auto nextImageInfo =
            vk::AcquireNextImageInfoKHR()
                .setSemaphore(semaphore)
                .setSwapchain(mSwapChain)
                .setTimeout(UINT64_MAX)
                .setDeviceMask(1);

        if (const auto imageIndexResult = mDevice->Get().acquireNextImage2KHR(nextImageInfo); imageIndexResult.result == vk::Result::eSuccess)
        {
            mCurrentFrameIndex = imageIndexResult.value;
            return mFrames[mCurrentFrameIndex];
        }

        return SwapChainFrame::Null;
    }

} // namespace FREYA_NAMESPACE
