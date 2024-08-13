#include "Builders/CommandPoolBuilder.hpp"

#include "Core/Device.hpp"
#include "Core/SwapChain.hpp"
#include <cassert>

namespace FREYA_NAMESPACE
{

    Ref<CommandPool> CommandPoolBuilder::Build()
    {
        auto queueFamilyIndices = mDevice->GetQueueFamilyIndices();

        const auto commandPoolCreateInfo =
            vk::CommandPoolCreateInfo()
                .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
                .setQueueFamilyIndex(queueFamilyIndices.graphicsFamily.value());

        auto commandPool = mDevice->Get().createCommandPool(commandPoolCreateInfo);

        assert(commandPool && "Failed to create command pool");

        const auto allocInfo = vk::CommandBufferAllocateInfo()
                                   .setCommandPool(commandPool)
                                   .setLevel(vk::CommandBufferLevel::ePrimary)
                                   .setCommandBufferCount(mCount);

        auto commandBuffers = mDevice->Get().allocateCommandBuffers(allocInfo);

        assert(!commandBuffers.empty() && "Failed to allocate command buffers");

        return MakeRef<CommandPool>(mDevice, commandPool, commandBuffers);
    }

} // namespace FREYA_NAMESPACE