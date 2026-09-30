#define VMA_IMPLEMENTATION
#include "Freya/Core/MemoryAllocator.hpp"

#include <cassert>

namespace FREYA_NAMESPACE
{
    MemoryAllocator::MemoryAllocator(VkInstance       instance,
                                     VkPhysicalDevice physicalDevice,
                                     VkDevice         device,
                                     std::uint32_t    apiVersion,
                                     bool             memoryPriorityExt)
    {
        VmaAllocatorCreateInfo createInfo {};
        createInfo.physicalDevice   = physicalDevice;
        createInfo.device           = device;
        createInfo.instance         = instance;
        createInfo.vulkanApiVersion = apiVersion;
        if (memoryPriorityExt)
            createInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;

        const VkResult result = vmaCreateAllocator(&createInfo, &mAllocator);
        assert(result == VK_SUCCESS && "vmaCreateAllocator failed.");
        if (result != VK_SUCCESS || mAllocator == VK_NULL_HANDLE)
            throw vk::SystemError(vk::Result::eErrorOutOfDeviceMemory,
                                  "Failed to create VMA allocator.");
    }

    MemoryAllocator::~MemoryAllocator()
    {
        if (mAllocator != VK_NULL_HANDLE)
        {
            vmaDestroyAllocator(mAllocator);
            mAllocator = VK_NULL_HANDLE;
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
            vmaCreateBuffer(mAllocator, &createInfo, &allocInfo, &out.buffer,
                            &out.allocation, &out.info);
        assert(result == VK_SUCCESS && "vmaCreateBuffer failed.");
        if (result != VK_SUCCESS)
            throw vk::SystemError(vk::Result::eErrorOutOfDeviceMemory,
                                  "VMA failed to create buffer.");
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
            vmaCreateImage(mAllocator, &createInfo, &allocInfo, &out.image,
                           &out.allocation, nullptr);
        assert(result == VK_SUCCESS && "vmaCreateImage failed.");
        if (result != VK_SUCCESS)
            throw vk::SystemError(vk::Result::eErrorOutOfDeviceMemory,
                                  "VMA failed to create image.");
        return out;
    }

    void MemoryAllocator::DestroyBuffer(VkBuffer      buffer,
                                        VmaAllocation allocation)
    {
        if (buffer == VK_NULL_HANDLE)
            return;
        vmaDestroyBuffer(mAllocator, buffer, allocation);
    }

    void MemoryAllocator::DestroyImage(VkImage image, VmaAllocation allocation)
    {
        if (image == VK_NULL_HANDLE)
            return;
        vmaDestroyImage(mAllocator, image, allocation);
    }

    void MemoryAllocator::Flush(VmaAllocation allocation, VkDeviceSize offset,
                                VkDeviceSize size)
    {
        vmaFlushAllocation(mAllocator, allocation, offset, size);
    }

} // namespace FREYA_NAMESPACE
