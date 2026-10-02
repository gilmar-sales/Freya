#pragma once

#include <cstddef>

namespace FREYA_NAMESPACE
{
    /**
     * Presentation wait semaphores must follow swapchain-image lifetime,
     * unlike acquisition semaphores and fences which follow flight slots.
     */
    struct SwapChainSync
    {
        [[nodiscard]] static constexpr std::size_t PresentationSemaphoreCount(
            std::size_t imageCount)
        {
            return imageCount;
        }

        [[nodiscard]] static constexpr std::size_t PresentationSemaphoreIndex(
            std::size_t imageIndex)
        {
            return imageIndex;
        }
    };
} // namespace FREYA_NAMESPACE
