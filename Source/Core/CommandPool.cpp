#include "Core/CommandPool.hpp"

#include "Core/Device.hpp"

namespace FREYA_NAMESPACE
{

    CommandPool::~CommandPool()
    {
        mDevice->Get().destroyCommandPool(mCommandPool);
    }

    vk::CommandBuffer CommandPool::CreateCommandBuffer() const
    {
        const auto allocInfo = vk::CommandBufferAllocateInfo()
                                   .setLevel(vk::CommandBufferLevel::ePrimary)
                                   .setCommandPool(mCommandPool)
                                   .setCommandBufferCount(1);

        return mDevice->Get().allocateCommandBuffers(allocInfo)[0];
    }

    void CommandPool::FreeCommandBuffer(const vk::CommandBuffer commandBuffer) const
    {
        mDevice->Get().freeCommandBuffers(mCommandPool, 1, &commandBuffer);
    }

} // namespace FREYA_NAMESPACE