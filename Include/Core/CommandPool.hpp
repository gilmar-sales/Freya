#pragma once

#include "vulkan/vulkan_handles.hpp"
#include <cassert>
#include <memory>
namespace FREYA_NAMESPACE
{
    class Device;

    class CommandPool
    {
      public:
        CommandPool(Ref<Device> device,
                    vk::CommandPool commandPool,
                    std::vector<vk::CommandBuffer> commandBuffers)
            : mDevice(device), mCommandPool(commandPool), mCommandBuffers(commandBuffers),
              mIndex(0)
        {
        }

        ~CommandPool();

        vk::CommandPool &Get() { return mCommandPool; }

        vk::CommandBuffer &GetCommandBuffer() { return mCommandBuffers[mIndex]; }

        void SetCommandBufferIndex(std::uint32_t index)
        {
            assert(index < mCommandBuffers.size() &&
                   "Cannot get vk::CommandBuffer: index out of bounds.");

            mIndex = index;
        }

        vk::CommandBuffer CreateCommandBuffer();
        void FreeCommandBuffer(vk::CommandBuffer);

      private:
        Ref<Device> mDevice;

        vk::CommandPool mCommandPool;
        std::vector<vk::CommandBuffer> mCommandBuffers;
        std::uint32_t mIndex;
    };

} // namespace FREYA_NAMESPACE