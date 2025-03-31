#include "Freya/Builders/CommandPoolBuilder.hpp"

#include "Freya/Core/Device.hpp"
#include "Freya/Core/SwapChain.hpp"
#include <cassert>

namespace FREYA_NAMESPACE
{

    Ref<CommandPool> CommandPoolBuilder::Build()
    {
        const auto queueFamilyIndices = mDevice->GetQueueFamilyIndices();

        const auto commandPoolCreateInfo =
            vk::CommandPoolCreateInfo()
                .setQueueFamilyIndex(queueFamilyIndices.graphicsFamily.value())
                .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

        auto commandPool =
            mDevice->Get().createCommandPool(commandPoolCreateInfo);

        assert(commandPool && "Failed to create command pool");

        const auto allocInfo =
            vk::CommandBufferAllocateInfo()
                .setCommandPool(commandPool)
                .setLevel(vk::CommandBufferLevel::ePrimary)
                .setCommandBufferCount(mCount);

        auto commandBuffers = mDevice->Get().allocateCommandBuffers(allocInfo);

        assert(!commandBuffers.empty() && "Failed to allocate command buffers");

        return skr::MakeRef<CommandPool>(mDevice, commandPool, commandBuffers);
    }

} // namespace FREYA_NAMESPACE