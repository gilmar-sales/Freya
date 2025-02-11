#include "Core/Buffer.hpp"

#include "Core/CommandPool.hpp"
#include "Core/Device.hpp"

namespace FREYA_NAMESPACE
{

    Buffer::~Buffer()
    {
        mDevice->Get().destroyBuffer(mBuffer);
        mDevice->Get().freeMemory(mMemory);
    }

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
