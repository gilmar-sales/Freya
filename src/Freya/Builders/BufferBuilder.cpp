#include "Freya/Builders/BufferBuilder.hpp"

#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/PhysicalDevice.hpp"

#include <cstring>
#include <limits>

namespace FREYA_NAMESPACE
{
    namespace
    {
        struct MemoryChoice
        {
            std::uint32_t typeIndex    = 0;
            bool          hostVisible  = false;
            bool          hostCoherent = false;
        };

        MemoryChoice ChooseBufferMemory(
            const skr::Arc<PhysicalDevice>& physical,
            const std::uint32_t             typeBits,
            const BufferUsage               usage)
        {
            using enum vk::MemoryPropertyFlagBits;

            const auto tryProps =
                [&](const vk::MemoryPropertyFlags props) -> std::uint32_t {
                std::uint32_t index = 0;
                if (physical->TryQueryCompatibleMemoryType(
                        typeBits, props, index))
                    return index;
                return std::numeric_limits<std::uint32_t>::max();
            };

            MemoryChoice choice {};

            if (usage == BufferUsage::Staging || usage == BufferUsage::Readback)
            {
                const auto idx = tryProps(eHostVisible | eHostCoherent);
                assert(idx != std::numeric_limits<std::uint32_t>::max());
                choice.typeIndex    = idx;
                choice.hostVisible  = true;
                choice.hostCoherent = true;
                return choice;
            }

            // Prefer ReBAR-style host-visible device-local + coherent so
            // Copy can write without flush or map/unmap.
            static constexpr vk::MemoryPropertyFlags kCandidates[] = {
                eHostVisible | eHostCoherent | eDeviceLocal,
                eHostVisible | eDeviceLocal,
                eHostVisible | eHostCoherent,
            };

            for (const auto props : kCandidates)
            {
                const auto idx = tryProps(props);
                if (idx == std::numeric_limits<std::uint32_t>::max())
                    continue;

                choice.typeIndex    = idx;
                choice.hostVisible  = true;
                choice.hostCoherent = (props & eHostCoherent) == eHostCoherent;
                return choice;
            }

            assert(!"Failed to find suitable buffer memory type.");
            return choice;
        }
    } // namespace

    skr::Arc<Buffer> BufferBuilder::Build()
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
                                    vk::BufferUsageFlagBits::eTransferDst);
                break;
            case BufferUsage::Indirect:
                bufferInfo.setUsage(vk::BufferUsageFlagBits::eIndirectBuffer |
                                    vk::BufferUsageFlagBits::eStorageBuffer |
                                    vk::BufferUsageFlagBits::eTransferDst);
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

        const auto choice =
            ChooseBufferMemory(mDevice->GetPhysicalDevice(),
                               memoryRequirements.memoryTypeBits, mUsage);

        auto priorityInfo =
            vk::MemoryPriorityAllocateInfoEXT().setPriority(0.2f);

        switch (mUsage)
        {
            case BufferUsage::Vertex:
            case BufferUsage::Index:
            case BufferUsage::Uniform:
            case BufferUsage::Instance:
            case BufferUsage::Storage:
            case BufferUsage::Indirect:
                priorityInfo.setPriority(1.0f);
                break;
            default:
                break;
        }

        const auto allocInfo =
            vk::MemoryAllocateInfo()
                .setAllocationSize(memoryRequirements.size)
                .setMemoryTypeIndex(choice.typeIndex)
                .setPNext(&priorityInfo);

        auto memory = mDevice->Get().allocateMemory(allocInfo);

        assert(memory && "Failed to allocate vk::DeviceMemory");

        mDevice->Get().bindBufferMemory(buffer, memory, 0);

        void* mapped = nullptr;
        if (choice.hostVisible && mSize > 0)
        {
            mapped = mDevice->Get().mapMemory(
                memory, 0, mSize, vk::MemoryMapFlagBits {});
            assert(mapped && "Failed to persistently map buffer memory");
        }

        if (mData != nullptr && mapped != nullptr)
        {
            std::memcpy(mapped, mData, mSize);
            if (!choice.hostCoherent)
            {
                const auto range =
                    vk::MappedMemoryRange()
                        .setMemory(memory)
                        .setOffset(0)
                        .setSize(mSize);
                mDevice->Get().flushMappedMemoryRanges(range);
            }
        }

        return skr::MakeArc<Buffer>(mDevice, mUsage, mSize, buffer, memory,
                                    mapped, choice.hostCoherent);
    };

} // namespace FREYA_NAMESPACE
