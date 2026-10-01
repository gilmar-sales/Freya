#include "Freya/Builders/BufferBuilder.hpp"

#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/MemoryAllocator.hpp"
#include "Freya/Core/PhysicalDevice.hpp"

#include <cstring>
#include <limits>

namespace FREYA_NAMESPACE
{
    namespace
    {
        struct MemoryChoice
        {
            MemoryUsage usage        = MemoryUsage::Upload;
            bool        hostCoherent = true;
        };

        MemoryChoice ChooseBufferMemory(const BufferUsage usage)
        {
            // Every Freya buffer is CPU-written at least once
            // (initial data + Copy ring updates), so keep them
            // host-visible via VMA CPU_TO_GPU / GPU_TO_CPU.
            // VMA sub-allocates from host-visible blocks (ReBAR
            // device-local when available) — no more 1:1
            // vkAllocateMemory per buffer.
            if (usage == BufferUsage::Readback)
                return { MemoryUsage::Readback, true };
            return { MemoryUsage::Upload, true };
        }

        float BufferPriority(const BufferUsage usage)
        {
            switch (usage)
            {
                case BufferUsage::Vertex:
                case BufferUsage::Index:
                case BufferUsage::Uniform:
                case BufferUsage::Instance:
                case BufferUsage::Storage:
                case BufferUsage::Indirect:
                    return 1.0f;
                default:
                    return 0.2f;
            }
        }
    } // namespace

    skr::Arc<Buffer> BufferBuilder::Build()
    {
        assert(mDevice.get() &&
               "Cannot create fra::Buffer with an invalid fra::Device");
        if (mSize == 0)
            throw vk::SystemError(vk::Result::eErrorUnknown,
                                  "Cannot create zero-size Buffer.");

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
            case BufferUsage::Readback:
                bufferInfo.setUsage(vk::BufferUsageFlagBits::eTransferDst);
                break;
            case BufferUsage::Instance:
            case BufferUsage::Vertex:
                bufferInfo.setUsage(vk::BufferUsageFlagBits::eVertexBuffer |
                                    vk::BufferUsageFlagBits::eStorageBuffer |
                                    vk::BufferUsageFlagBits::eTransferDst |
                                    vk::BufferUsageFlagBits::eTransferSrc);
                break;
            case BufferUsage::Index:
                bufferInfo.setUsage(vk::BufferUsageFlagBits::eIndexBuffer |
                                    vk::BufferUsageFlagBits::eTransferDst |
                                    vk::BufferUsageFlagBits::eTransferSrc);
                break;
            case BufferUsage::Uniform:
                bufferInfo.setUsage(vk::BufferUsageFlagBits::eUniformBuffer);
                break;
            case BufferUsage::Storage:
                bufferInfo.setUsage(vk::BufferUsageFlagBits::eStorageBuffer |
                                    vk::BufferUsageFlagBits::eTransferDst |
                                    vk::BufferUsageFlagBits::eTransferSrc);
                break;
            case BufferUsage::Indirect:
                bufferInfo.setUsage(vk::BufferUsageFlagBits::eIndirectBuffer |
                                    vk::BufferUsageFlagBits::eStorageBuffer |
                                    vk::BufferUsageFlagBits::eTransferDst |
                                    vk::BufferUsageFlagBits::eTransferSrc);
                break;
            default:
                break;
        }

        // Keep concurrent sharing indices alive through createBuffer.
        // NOLINTNEXTLINE: synchronous use only.
        std::array<std::uint32_t, 2> concurrentQueues {};
        bool                         useConcurrent = false;
        if (queueFamilyIndices.isUnique())
        {
            concurrentQueues = { queueFamilyIndices.graphicsFamily.value(),
                                 queueFamilyIndices.transferFamily.value() };

            bufferInfo.setSharingMode(vk::SharingMode::eConcurrent)
                .setQueueFamilyIndexCount(2)
                .setPQueueFamilyIndices(concurrentQueues.data());
            useConcurrent = true;
            (void) useConcurrent;
        }

        VkBuffer          rawBuffer {};
        MemoryAllocation     allocation {};
        BufferAllocationInfo allocInfo {};
        try
        {
            const auto choice = ChooseBufferMemory(mUsage);

            auto& allocator = mDevice->GetAllocator();
            assert(allocator && "Device has no VMA MemoryAllocator.");

            const auto rawInfo = static_cast<VkBufferCreateInfo>(bufferInfo);
            auto       created = allocator->CreateBuffer(
                rawInfo, choice.usage, BufferPriority(mUsage), mSize > 0);
            rawBuffer  = created.buffer;
            allocation = created.allocation;
            allocInfo  = created.info;

            vk::Buffer buffer(static_cast<VkBuffer>(rawBuffer));
            assert(buffer && "VMA failed to create vk::Buffer.");

            void* mapped = allocInfo.mappedData;
            if (mSize > 0)
            {
                assert(mapped && "VMA did not persistently map buffer.");
                if (mapped == nullptr)
                    throw vk::SystemError(vk::Result::eErrorMemoryMapFailed,
                                          "Failed to map buffer memory.");
            }

            const bool hostCoherent = allocInfo.hostCoherent;

            if (mData != nullptr && mapped != nullptr)
            {
                std::memcpy(mapped, mData, mSize);
                if (!hostCoherent)
                    allocator->Flush(allocation, 0, mSize);
            }

            return skr::MakeArc<Buffer>(mDevice, mUsage, mSize, buffer,
                                        allocation, allocInfo, hostCoherent);
        }
        catch (...)
        {
            if (rawBuffer)
            {
                auto& allocator = mDevice->GetAllocator();
                if (allocator)
                    allocator->DestroyBuffer(rawBuffer, allocation);
            }
            throw;
        }
    };

} // namespace FREYA_NAMESPACE
