#include "Freya/Builders/BufferBuilder.hpp"

#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/PhysicalDevice.hpp"

namespace FREYA_NAMESPACE
{

    Ref<Buffer> BufferBuilder::Build()
    {
        assert(mDevice.get() &&
               "Cannot create fra::Buffer with an invalid fra::Device");

        const auto queueFamilyIndices = mDevice->GetQueueFamilyIndices();

        auto bufferInfo =
            vk::BufferCreateInfo()
                .setSize(mSize)
                .setSharingMode(vk::SharingMode::eExclusive)
                .setQueueFamilyIndexCount(1)
                .setPQueueFamilyIndices(
                    &queueFamilyIndices.graphicsFamily.value());

        switch (mUsage)
        {
            case BufferUsage::Staging:
                bufferInfo.setUsage(vk::BufferUsageFlagBits::eTransferSrc);
                break;
            case BufferUsage::Instance:
            case BufferUsage::Vertex:
                bufferInfo.setUsage(vk::BufferUsageFlagBits::eVertexBuffer |
                                    vk::BufferUsageFlagBits::eTransferDst);
                break;
            case BufferUsage::Index:
                bufferInfo.setUsage(vk::BufferUsageFlagBits::eIndexBuffer |
                                    vk::BufferUsageFlagBits::eTransferDst);
                break;
            case BufferUsage::Uniform:
                bufferInfo.setUsage(vk::BufferUsageFlagBits::eUniformBuffer);
                break;
            default:
                break;
        }

        if (queueFamilyIndices.isUnique())
        {
            const std::array queues = {
                queueFamilyIndices.graphicsFamily.value(),
                queueFamilyIndices.transferFamily.value()
            };

            bufferInfo.setSharingMode(vk::SharingMode::eConcurrent)
                .setQueueFamilyIndexCount(2)
                .setPQueueFamilyIndices(queues.data());
        }

        auto buffer = mDevice->Get().createBuffer(bufferInfo);

        assert(buffer && "Failed to create vk::Buffer.");

        const auto memoryRequirements =
            mDevice->Get().getBufferMemoryRequirements(buffer);

        auto memoryProperties = vk::MemoryPropertyFlags {};

        switch (mUsage)
        {
            case BufferUsage::Staging:
                memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible |
                                   vk::MemoryPropertyFlagBits::eHostCoherent;
                break;
            case BufferUsage::Vertex:
            case BufferUsage::Index:
            case BufferUsage::Uniform:
            case BufferUsage::Instance:
                memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible |
                                   vk::MemoryPropertyFlagBits::eDeviceLocal;
                break;
            default:
                break;
        }

        const auto memoryTypeIndex =
            mDevice->GetPhysicalDevice()->QueryCompatibleMemoryType(
                memoryRequirements.memoryTypeBits,
                memoryProperties);

        auto priorityInfo =
            vk::MemoryPriorityAllocateInfoEXT().setPriority(0.2f);

        switch (mUsage)
        {
            case BufferUsage::Vertex:
            case BufferUsage::Index:
            case BufferUsage::Uniform:
            case BufferUsage::Instance:
                priorityInfo.setPriority(1.0f);
                break;
            default:
                break;
        }

        const auto allocInfo =
            vk::MemoryAllocateInfo()
                .setAllocationSize(memoryRequirements.size)
                .setMemoryTypeIndex(memoryTypeIndex)
                .setPNext(&priorityInfo);

        auto memory = mDevice->Get().allocateMemory(allocInfo);

        assert(memory && "Failed to allocate vk::DeviceMemory");

        mDevice->Get().bindBufferMemory(buffer, memory, 0);

        if (mData != nullptr)
        {
            void* data = mDevice->Get().mapMemory(
                memory,
                0,
                mSize,
                vk::MemoryMapFlagBits {});

            memcpy(data, mData, mSize);

            mDevice->Get().unmapMemory(memory);
        }

        return MakeRef<Buffer>(mDevice, mUsage, mSize, buffer, memory);
    };

} // namespace FREYA_NAMESPACE
