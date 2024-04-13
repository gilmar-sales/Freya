#include "Builders/BufferBuilder.hpp"

#include "Core/Buffer.hpp"
#include "Core/Device.hpp"
#include "Core/PhysicalDevice.hpp"

namespace FREYA_NAMESPACE
{

    std::shared_ptr<Buffer> BufferBuilder::Build()
    {
        assert(mDevice.get() && "Cannot create fra::Buffer with an invalid fra::Device");

        auto queueFamilyIndices = mDevice->GetQueueFamilyIndices();

        auto bufferInfo =
            vk::BufferCreateInfo()
                .setSize(mSize)
                .setSharingMode(vk::SharingMode::eExclusive)
                .setQueueFamilyIndexCount(1)
                .setPQueueFamilyIndices(&queueFamilyIndices.graphicsFamily.value());

        switch (mUsage)
        {
        case fra::BufferUsage::Staging:
            bufferInfo.setUsage(vk::BufferUsageFlagBits::eTransferSrc);
            break;
        case fra::BufferUsage::Vertex:
            bufferInfo.setUsage(vk::BufferUsageFlagBits::eVertexBuffer |
                                vk::BufferUsageFlagBits::eTransferDst);
            break;
        case fra::BufferUsage::Index:
            bufferInfo.setUsage(vk::BufferUsageFlagBits::eIndexBuffer |
                                vk::BufferUsageFlagBits::eTransferDst);
            break;
        case BufferUsage::Uniform:
            bufferInfo.setUsage(vk::BufferUsageFlagBits::eUniformBuffer);
            break;
        }

        if (queueFamilyIndices.isUnique())
        {
            std::array<std::uint32_t, 2> queues = {
                queueFamilyIndices.graphicsFamily.value(),
                queueFamilyIndices.transferFamily.value()};

            bufferInfo.setSharingMode(vk::SharingMode::eConcurrent)
                .setQueueFamilyIndexCount(2)
                .setPQueueFamilyIndices(queues.data());
        }

        auto buffer = mDevice->Get().createBuffer(bufferInfo);

        assert(buffer && "Failed to create vk::Buffer.");

        auto memoryRequirements = mDevice->Get().getBufferMemoryRequirements(buffer);

        auto allocInfo =
            vk::MemoryAllocateInfo()
                .setAllocationSize(memoryRequirements.size)
                .setMemoryTypeIndex(
                    mDevice->GetPhysicalDevice()->QueryCompatibleMemoryType(
                        memoryRequirements.memoryTypeBits,
                        vk::MemoryPropertyFlagBits::eHostVisible |
                            vk::MemoryPropertyFlagBits::eHostCoherent));

        auto memory = mDevice->Get().allocateMemory(allocInfo);

        assert(memory && "Failed to allocate vk::DeviceMemory");

        mDevice->Get().bindBufferMemory(buffer, memory, 0);

        if (mData != nullptr)
        {
            void *data =
                mDevice->Get().mapMemory(memory, 0, mSize, vk::MemoryMapFlagBits{});

            memcpy(data, mData, mSize);

            mDevice->Get().unmapMemory(memory);
        }

        return std::make_shared<Buffer>(mDevice, mUsage, mSize, buffer, memory);
    };

} // namespace FREYA_NAMESPACE
