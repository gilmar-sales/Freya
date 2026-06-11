#include "Freya/Core/Buffer.hpp"

#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Destroys the Vulkan buffer and frees device memory.
     */
    Buffer::~Buffer()
    {
        mDevice->Get().waitIdle();
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
     */
    void Buffer::Bind() const
    {
        constexpr vk::DeviceSize offsets[] = { 0 };
        switch (mUsage)
        {
            case BufferUsage::Vertex:
                mCommandPool->GetCommandBuffer().bindVertexBuffers(
                    0, 1, &mBuffer, offsets);
                break;
            case BufferUsage::Index:
                mCommandPool->GetCommandBuffer().bindIndexBuffer(
                    mBuffer, 0, vk::IndexType::eUint16);
                break;
            case BufferUsage::Instance: {
                mCommandPool->GetCommandBuffer().bindVertexBuffers(
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
        // Only Staging and Uniform buffers have HOST_VISIBLE memory that can
        // be mapped. Vertex, Index, Instance buffers are DEVICE_LOCAL only.
        if (mSize >= size && data != nullptr &&
            (mUsage == BufferUsage::Staging || mUsage == BufferUsage::Uniform))
        {
            void* deviceData = mDevice->Get().mapMemory(
                mMemory, offset, size, vk::MemoryMapFlagBits {});

            memcpy(deviceData, data, size);

            mDevice->Get().unmapMemory(mMemory);
            return;
        }

        const auto copyRegion =
            vk::BufferCopy().setSrcOffset(0).setDstOffset(offset).setSize(size);

        auto stagingBuffer =
            mServiceProvider->GetService<BufferBuilder>()
                ->SetData(data)
                .SetSize(size)
                .SetUsage(BufferUsage::Staging)
                .Build();
        constexpr auto beginInfo = vk::CommandBufferBeginInfo().setFlags(
            vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        const auto copyCmd = mCommandPool->CreateCommandBuffer();

        copyCmd.begin(beginInfo);
        copyCmd.copyBuffer(
            stagingBuffer->Get(), // src (HOST_VISIBLE staging)
            mBuffer,              // dst (DEVICE_LOCAL)
            1,
            &copyRegion);
        copyCmd.end();

        const auto submitInfo = vk::SubmitInfo().setCommandBuffers(copyCmd);

        if (mDevice->GetGraphicsQueue().submit(1, &submitInfo, nullptr) !=
            vk::Result::eSuccess)
            throw std::runtime_error("failed to submit buffer copy!");

        mDevice->GetGraphicsQueue().waitIdle();

        mCommandPool->FreeCommandBuffer(copyCmd);
    }

} // namespace FREYA_NAMESPACE
