#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace FREYA_NAMESPACE
{
    class Device;

    /**
     * @brief Usage class for VMA sub-allocation.
     *
     * Maps to VMA memory usage plus host-access flags so callers
     * never touch VmaAllocationCreateInfo directly.
     */
    enum class MemoryUsage
    {
        GpuOnly, ///< Device-local, no CPU access (attachments, textures)
        Upload,  ///< CPU-to-GPU, persistently mapped (all Buffer writes)
        Readback ///< GPU-to-CPU, persistently mapped (pick/cull readback)
    };

    /**
     * @brief Result of a VMA buffer allocation.
     */
    struct BufferAllocation
    {
        VkBuffer          buffer     = VK_NULL_HANDLE;
        VmaAllocation     allocation = VK_NULL_HANDLE;
        VmaAllocationInfo info {};
    };

    /**
     * @brief Result of a VMA image allocation.
     */
    struct ImageAllocation
    {
        VkImage       image      = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
    };

    /**
     * @brief Thin RAII wrapper around VmaAllocator.
     *
     * Owned by fra::Device (see Device::GetAllocator). All GPU
     * memory in Freya flows through here: BufferBuilder,
     * ImageBuilder, shadow arrays, Hi-Z, fallback textures.
     * VMA sub-allocates small resources from large blocks, which
     * silences BestPractices small-dedicated-allocation warnings
     * and reuses freed ranges instead of vkAllocate/vkFree churn.
     *
     * Priority (0..1, VK_EXT_memory_priority) is forwarded to VMA
     * so NVIDIA eviction keeps attachments/storage resident.
     */
    class MemoryAllocator
    {
      public:
        MemoryAllocator(VkInstance       instance,
                        VkPhysicalDevice physicalDevice,
                        VkDevice         device,
                        std::uint32_t    apiVersion,
                        bool             memoryPriorityExt);

        ~MemoryAllocator();

        MemoryAllocator(const MemoryAllocator&)            = delete;
        MemoryAllocator& operator=(const MemoryAllocator&) = delete;

        BufferAllocation CreateBuffer(const VkBufferCreateInfo& createInfo,
                                      MemoryUsage usage, float priority,
                                      bool map);

        ImageAllocation CreateImage(const VkImageCreateInfo& createInfo,
                                    float                    priority);

        void DestroyBuffer(VkBuffer buffer, VmaAllocation allocation);
        void DestroyImage(VkImage image, VmaAllocation allocation);

        void Flush(VmaAllocation allocation,
                   VkDeviceSize  offset,
                   VkDeviceSize  size);

        [[nodiscard]] VmaAllocator Get() const { return mAllocator; }

      private:
        VmaAllocator mAllocator = VK_NULL_HANDLE;
    };

} // namespace FREYA_NAMESPACE
