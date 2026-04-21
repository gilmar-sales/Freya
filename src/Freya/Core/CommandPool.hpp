#pragma once

#include "vulkan/vulkan_handles.hpp"

namespace FREYA_NAMESPACE
{
    class Device;

    /**
     * @brief Manages command buffer allocation and pool lifecycle.
     *
     * Wraps a Vulkan command pool with pre-allocated command buffers.
     * Provides round-robin access to command buffers via index.
     *
     * @param device        Device reference
     * @param commandPool   Vulkan command pool handle
     * @param commandBuffers Pre-allocated command buffer vector
     */
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

        /**
         * @brief Returns the underlying command pool handle.
         */
        vk::CommandPool& Get() { return mCommandPool; }

        /**
         * @brief Returns the command buffer at current index.
         * @note Index wraps based on frame count
         */
        vk::CommandBuffer& GetCommandBuffer()
        {
            return mCommandBuffers[mIndex];
        }

        /**
         * @brief Sets the command buffer index for round-robin access.
         * @param index Must be less than command buffer count
         * @throws Assertion if index is out of bounds
         */
        void SetCommandBufferIndex(const std::uint32_t index)
        {
            assert(index < mCommandBuffers.size() &&
                   "Cannot get vk::CommandBuffer: index out of bounds.");

            mIndex = index;
        }

        /**
         * @brief Allocates a new command buffer from the pool.
         * @return Newly allocated primary command buffer
         */
        [[nodiscard]] vk::CommandBuffer CreateCommandBuffer() const;

        /**
         * @brief Frees a command buffer back to the pool.
         * @param commandBuffer Command buffer to free
         */
        void FreeCommandBuffer(vk::CommandBuffer) const;

      private:
        Ref<Device> mDevice;

        vk::CommandPool                mCommandPool;
        std::vector<vk::CommandBuffer> mCommandBuffers;
        std::uint32_t                  mIndex;
    };

} // namespace FREYA_NAMESPACE