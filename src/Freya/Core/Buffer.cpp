#include "Freya/Core/Buffer.hpp"

#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Destroys the Vulkan buffer and frees device memory.
     */
    Buffer::~Buffer()
    {
        mDevice->Get().destroyBuffer(mBuffer);
        mDevice->Get().freeMemory(mMemory);
    }

    /**
     * @brief Binds this buffer to the current command buffer.
     *
     * Uses buffer usage type to determine binding method:
     * - Vertex: bindVertexBuffers at binding 0
     * - Index: bindIndexBuffer
     * - Instance: bindVertexBuffers at binding 1
     *
     * @param commandPool Command pool with current command buffer
     */
    void Buffer::Bind(const Ref<CommandPool>& commandPool) const
    {
        constexpr vk::DeviceSize offsets[] = { 0 };
        switch (mUsage)
        {
            case BufferUsage::Vertex:
                commandPool->GetCommandBuffer().bindVertexBuffers(
                    0, 1, &mBuffer, offsets);
                break;
            case BufferUsage::Index:
                commandPool->GetCommandBuffer().bindIndexBuffer(
                    mBuffer, 0, vk::IndexType::eUint16);
                break;
            case BufferUsage::Instance: {
                commandPool->GetCommandBuffer().bindVertexBuffers(
                    1, 1, &mBuffer, offsets);
            }
            default:
                break;
        }
    }

    /**
     * @brief Copies data into buffer memory via mapping.
     *
     * Only performs copy if size fits within buffer and data is not null.
     * Uses memcpy to transfer data to device memory.
     *
     * @param data  Source data pointer
     * @param size  Size of data to copy
     * @param offset Offset into buffer (default 0)
     */
    void Buffer::Copy(const void* data, const std::uint64_t size,
                      const std::uint64_t offset)
    {
        if (mSize >= size && data != nullptr)
        {
            void* deviceData = mDevice->Get().mapMemory(
                mMemory, offset, size, vk::MemoryMapFlagBits {});

            memcpy(deviceData, data, size);

            mDevice->Get().unmapMemory(mMemory);
        }
    }

} // namespace FREYA_NAMESPACE
