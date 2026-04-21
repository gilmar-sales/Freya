#include "CommandPool.hpp"

#include "Freya/Core/Device.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Destroys the Vulkan command pool.
     */
    CommandPool::~CommandPool()
    {
        mDevice->Get().destroyCommandPool(mCommandPool);
    }

    /**
     * @brief Allocates a new primary command buffer from the pool.
     * @return Newly allocated command buffer
     */
    vk::CommandBuffer CommandPool::CreateCommandBuffer() const
    {
        const auto allocInfo =
            vk::CommandBufferAllocateInfo()
                .setLevel(vk::CommandBufferLevel::ePrimary)
                .setCommandPool(mCommandPool)
                .setCommandBufferCount(1);

        return mDevice->Get().allocateCommandBuffers(allocInfo)[0];
    }

    /**
     * @brief Frees a command buffer back to the pool.
     * @param commandBuffer Command buffer to free
     */
    void CommandPool::FreeCommandBuffer(
        const vk::CommandBuffer commandBuffer) const
    {
        mDevice->Get().freeCommandBuffers(mCommandPool, 1, &commandBuffer);
    }

} // namespace FREYA_NAMESPACE
