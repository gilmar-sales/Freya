#include <Freya/Core/SwapChainSync.hpp>

#include <gtest/gtest.h>

TEST(SwapChainSync, PresentationSemaphoresAreAllocatedPerSwapchainImage)
{
    // The implementation may provide more images than the requested number
    // of frames in flight.
    EXPECT_EQ(fra::SwapChainSync::PresentationSemaphoreCount(4u), 4u);
    EXPECT_EQ(fra::SwapChainSync::PresentationSemaphoreCount(3u), 3u);
}

TEST(SwapChainSync, PresentationSemaphoreFollowsAcquiredImageNotFrameSlot)
{
    constexpr std::size_t frameSlot = 1u;
    constexpr std::size_t acquiredImage = 2u;

    EXPECT_EQ(fra::SwapChainSync::PresentationSemaphoreIndex(acquiredImage),
              acquiredImage);
    EXPECT_NE(fra::SwapChainSync::PresentationSemaphoreIndex(acquiredImage),
              frameSlot);
}
