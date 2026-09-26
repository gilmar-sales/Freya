#include "SwapChain.hpp"

namespace FREYA_NAMESPACE
{
    SwapChainFrame SwapChainFrame::Null = SwapChainFrame {};

    SwapChain::~SwapChain()
    {
        mDevice->Get().waitIdle();
        for (const auto& frame : mFrames)
        {
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
        const auto nextImageInfo =
            vk::AcquireNextImageInfoKHR()
                .setSwapchain(mSwapChain)
                .setTimeout(UINT64_MAX)
                .setSemaphore(mImageAvailableSemaphores[mCurrentFrameIndex])
                .setDeviceMask(1);

        const auto imageIndexResult =
            mDevice->Get().acquireNextImage2KHR(nextImageInfo);

        if (imageIndexResult.result != vk::Result::eSuccess &&
            imageIndexResult.result != vk::Result::eSuboptimalKHR)
        {
            throw std::runtime_error("Failed to acquire next image!");
        }

        mCurrentImageIndex = imageIndexResult.value;
        // The flight slot may be free while this image is still presented
        // by an earlier frame (image count can exceed the slot count).
        if (const auto imageFence = mImagesInFlight[mCurrentImageIndex])
        {
            if (mDevice->Get().waitForFences(
                    1, &imageFence, true, UINT64_MAX) != vk::Result::eSuccess)
                throw std::runtime_error("failed to wait for image fence!");
        }
        return mFrames[mCurrentImageIndex];
    }

    void SwapChain::WaitCommandBuffersSubmission(
        std::vector<vk::CommandBuffer> commandBuffers)
    {
    }

    vk::Result SwapChain::Present(std::vector<vk::CommandBuffer> commandBuffers)
    {
        std::vector<vk::PipelineStageFlags> waitStages = {
            vk::PipelineStageFlagBits::eColorAttachmentOutput
        };

        std::vector<vk::Semaphore> waitSemaphores = {
            mImageAvailableSemaphores[mCurrentFrameIndex]
        };

        std::vector<vk::Semaphore> signalSemaphores = {
            mRenderFinishedSemaphores[mCurrentFrameIndex]
        };

        vk::SubmitInfo submitInfo = {};
        submitInfo.setCommandBuffers(commandBuffers)
            .setWaitSemaphores(waitSemaphores)
            .setWaitDstStageMask(waitStages)
            .setSignalSemaphores(signalSemaphores);

        const auto submitResult = mDevice->GetGraphicsQueue().submit(
            1,
            &submitInfo,
            mInFlightFences[mCurrentFrameIndex]);

        if (submitResult != vk::Result::eSuccess)
            throw std::runtime_error("failed to submit draw command buffer!");

        std::vector<vk::SwapchainKHR> swapChains = { mSwapChain };

        std::vector<uint32_t> imageIndices = { mCurrentImageIndex };

        vk::PresentInfoKHR presentInfo = {};
        presentInfo.setWaitSemaphores(signalSemaphores)
            .setSwapchains(swapChains)
            .setImageIndices(imageIndices);

        const auto result = mDevice->GetPresentQueue().presentKHR(presentInfo);

        // Tag the image with this slot's fence so a later frame acquiring
        // the same image waits for this presentation to finish.
        mImagesInFlight[mCurrentImageIndex] =
            mInFlightFences[mCurrentFrameIndex];

        mCurrentFrameIndex = (mCurrentFrameIndex + 1) % mInFlightFences.size();

        return result;
    }

} // namespace FREYA_NAMESPACE
