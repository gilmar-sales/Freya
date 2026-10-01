#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#include "Freya/Core/MemoryAllocator.hpp"

#include <cassert>

namespace FREYA_NAMESPACE
{
    struct MemoryAllocator::Impl
    {
        VmaAllocator allocator = VK_NULL_HANDLE;
    };

    MemoryAllocator::MemoryAllocator(VkInstance       instance,
                                     VkPhysicalDevice physicalDevice,
                                     VkDevice         device,
                                     std::uint32_t    apiVersion,
                                     bool             memoryPriorityExt)
    {
        mImpl = new Impl;
        VmaAllocatorCreateInfo createInfo {};
        createInfo.physicalDevice   = physicalDevice;
        createInfo.device           = device;
        createInfo.instance         = instance;
        createInfo.vulkanApiVersion = apiVersion;
        if (memoryPriorityExt)
            createInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;

        const VkResult result = vmaCreateAllocator(&createInfo, &mImpl->allocator);
        assert(result == VK_SUCCESS && "vmaCreateAllocator failed.");
        if (result != VK_SUCCESS || mImpl->allocator == VK_NULL_HANDLE)
            throw vk::SystemError(vk::Result::eErrorOutOfDeviceMemory,
                                  "Failed to create VMA allocator.");
    }

    MemoryAllocator::~MemoryAllocator()
    {
        if (mImpl != nullptr)
        {
            if (mImpl->allocator != VK_NULL_HANDLE)
                vmaDestroyAllocator(mImpl->allocator);
            delete mImpl;
            mImpl = nullptr;
        }
    }

    BufferAllocation MemoryAllocator::CreateBuffer(
        const VkBufferCreateInfo& createInfo, const MemoryUsage usage,
        const float priority, const bool map)
    {
        VmaAllocationCreateInfo allocInfo {};
        allocInfo.priority = priority;

        switch (usage)
        {
            case MemoryUsage::GpuOnly:
                allocInfo.usage         = VMA_MEMORY_USAGE_GPU_ONLY;
                allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                break;
            case MemoryUsage::Upload:
                allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
                allocInfo.flags =
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                    (map ? VMA_ALLOCATION_CREATE_MAPPED_BIT : 0u);
                break;
            case MemoryUsage::Readback:
                allocInfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
                allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                                  (map ? VMA_ALLOCATION_CREATE_MAPPED_BIT : 0u);
                break;
        }

        BufferAllocation out {};
        const VkResult   result =
            vmaCreateBuffer(mImpl->allocator, &createInfo, &allocInfo, &out.buffer,
                            reinterpret_cast<VmaAllocation*>(&out.allocation.mHandle), nullptr);
        assert(result == VK_SUCCESS && "vmaCreateBuffer failed.");
        if (result != VK_SUCCESS)
            throw vk::SystemError(vk::Result::eErrorOutOfDeviceMemory,
                                  "VMA failed to create buffer.");
        VmaAllocationInfo vmaInfo {};
        vmaGetAllocationInfo(mImpl->allocator,
                             static_cast<VmaAllocation>(out.allocation.mHandle),
                             &vmaInfo);
        out.info.mappedData = vmaInfo.pMappedData;
        VkMemoryPropertyFlags properties {};
        vmaGetAllocationMemoryProperties(mImpl->allocator,
            static_cast<VmaAllocation>(out.allocation.mHandle), &properties);
        out.info.hostCoherent = (properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
        return out;
    }

    ImageAllocation MemoryAllocator::CreateImage(
        const VkImageCreateInfo& createInfo, const float priority)
    {
        VmaAllocationCreateInfo allocInfo {};
        allocInfo.usage         = VMA_MEMORY_USAGE_GPU_ONLY;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        allocInfo.priority      = priority;

        ImageAllocation out {};
        const VkResult  result =
            vmaCreateImage(mImpl->allocator, &createInfo, &allocInfo, &out.image,
                           reinterpret_cast<VmaAllocation*>(&out.allocation.mHandle), nullptr);
        assert(result == VK_SUCCESS && "vmaCreateImage failed.");
        if (result != VK_SUCCESS)
            throw vk::SystemError(vk::Result::eErrorOutOfDeviceMemory,
                                  "VMA failed to create image.");
        return out;
    }

    void MemoryAllocator::DestroyBuffer(VkBuffer      buffer,
                                        MemoryAllocation allocation)
    {
        if (buffer == VK_NULL_HANDLE)
            return;
        vmaDestroyBuffer(mImpl->allocator, buffer, static_cast<VmaAllocation>(allocation.mHandle));
    }

    void MemoryAllocator::DestroyImage(VkImage image, MemoryAllocation allocation)
    {
        if (image == VK_NULL_HANDLE)
            return;
        vmaDestroyImage(mImpl->allocator, image, static_cast<VmaAllocation>(allocation.mHandle));
    }

    void MemoryAllocator::Flush(MemoryAllocation allocation, VkDeviceSize offset,
                                VkDeviceSize size)
    {
        vmaFlushAllocation(mImpl->allocator, static_cast<VmaAllocation>(allocation.mHandle), offset, size);
    }

    void* MemoryAllocator::Map(MemoryAllocation allocation)
    {
        void* mapped = nullptr;
        if (vmaMapMemory(mImpl->allocator,
                         static_cast<VmaAllocation>(allocation.mHandle),
                         &mapped) != VK_SUCCESS)
            return nullptr;
        return mapped;
    }

    void MemoryAllocator::Unmap(MemoryAllocation allocation)
    {
        vmaUnmapMemory(mImpl->allocator,
                       static_cast<VmaAllocation>(allocation.mHandle));
    }

} // namespace FREYA_NAMESPACE
