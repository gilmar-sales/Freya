#include "Freya/Core/SwapChain.hpp"

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

        for (const auto& semaphore : mRenderFinishedSemaphores)
        {
            mDevice->Get().destroySemaphore(semaphore);
        }

        for (const auto& semaphore : mImageAvailableSemaphores)
        {
            mDevice->Get().destroySemaphore(semaphore);
        }

        for (const auto& fence : mInFlightFences)
        {
            mDevice->Get().destroyFence(fence);
        }
    }

    void SwapChain::WaitNextFrame()
    {
        if (mDevice->Get().waitForFences(1,
                                         &mInFlightFences[mCurrentFrameIndex],
                                         true,
                                         UINT64_MAX) != vk::Result::eSuccess)
            throw std::runtime_error("failed to wait for fences!");
    }

    void SwapChain::BeginNextFrame()
    {
        if (mDevice->Get().resetFences(1,
                                       &mInFlightFences[mCurrentFrameIndex]) !=
            vk::Result::eSuccess)
            throw std::runtime_error("failed to reset fences!");
    }

    const SwapChainFrame& SwapChain::GetNextFrame()
    {

        auto semaphore = mImageAvailableSemaphores[mCurrentFrameIndex];

        const auto nextImageInfo =
            vk::AcquireNextImageInfoKHR()
                .setSemaphore(semaphore)
                .setSwapchain(mSwapChain)
                .setTimeout(UINT64_MAX)
                .setDeviceMask(1);

        if (const auto imageIndexResult =
                mDevice->Get().acquireNextImage2KHR(nextImageInfo);
            imageIndexResult.result == vk::Result::eSuccess)
        {
            mCurrentFrameIndex = imageIndexResult.value;
            return mFrames[mCurrentFrameIndex];
        }

        return SwapChainFrame::Null;
    }

    void SwapChain::WaitCommandBuffersSubmission(
        std::vector<vk::CommandBuffer> commandBuffers)
    {
        auto waitSemaphores = { mImageAvailableSemaphores[mCurrentFrameIndex] };

        auto signalSemaphores = {
            mRenderFinishedSemaphores[mCurrentFrameIndex]
        };

        constexpr vk::PipelineStageFlags waitStages[] = {
            vk::PipelineStageFlagBits::eColorAttachmentOutput
        };
        const auto submitInfo =
            vk::SubmitInfo()
                .setWaitSemaphores(waitSemaphores)
                .setWaitDstStageMask(waitStages)
                .setCommandBuffers(commandBuffers)
                .setSignalSemaphores(signalSemaphores);

        const auto submitResult = mDevice->GetGraphicsQueue().submit(
            1,
            &submitInfo,
            mInFlightFences[mCurrentFrameIndex]);

        if (submitResult != vk::Result::eSuccess)
            throw std::runtime_error("failed to submit draw command buffer!");
    }

    vk::Result SwapChain::Present()
    {

        auto swapChains = { mSwapChain };

        auto imageIndices = { mCurrentFrameIndex };

        auto signalSemaphores = {
            mRenderFinishedSemaphores[mCurrentFrameIndex]
        };

        const auto presentInfo =
            vk::PresentInfoKHR()
                .setWaitSemaphores(signalSemaphores)
                .setSwapchains(swapChains)
                .setImageIndices(imageIndices);

        const auto result = mDevice->GetPresentQueue().presentKHR(presentInfo);

        mCurrentFrameIndex = (mCurrentFrameIndex + 1) % mFrames.size();

        return result;
    }

} // namespace FREYA_NAMESPACE
