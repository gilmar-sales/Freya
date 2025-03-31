#pragma once

#include "vulkan/vulkan_handles.hpp"

namespace FREYA_NAMESPACE
{
    class Device;

    class CommandPool
    {
      public:
        CommandPool(const Ref<Device>&                    device,
                    const vk::CommandPool                 commandPool,
                    const std::vector<vk::CommandBuffer>& commandBuffers) :
            mDevice(device), mCommandPool(commandPool),
            mCommandBuffers(commandBuffers), mIndex(0)
        {
        }

        ~CommandPool();

        vk::CommandPool& Get() { return mCommandPool; }

        vk::CommandBuffer& GetCommandBuffer()
        {
            return mCommandBuffers[mIndex];
        }

        void SetCommandBufferIndex(const std::uint32_t index)
        {
            assert(index < mCommandBuffers.size() &&
                   "Cannot get vk::CommandBuffer: index out of bounds.");

            mIndex = index;
        }

        [[nodiscard]] vk::CommandBuffer CreateCommandBuffer() const;
        void FreeCommandBuffer(vk::CommandBuffer) const;

      private:
        Ref<Device> mDevice;

        vk::CommandPool                mCommandPool;
        std::vector<vk::CommandBuffer> mCommandBuffers;
        std::uint32_t                  mIndex;
    };

} // namespace FREYA_NAMESPACE