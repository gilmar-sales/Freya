#pragma once

#include <vulkan/vulkan.h>

namespace FREYA_NAMESPACE
{
    class Device;

    /**
     * @brief Usage class for VMA sub-allocation.
     *
     * Maps to VMA memory usage plus host-access flags so callers
     * never touch MemoryAllocationCreateInfo directly.
     */
    enum class MemoryUsage
    {
        GpuOnly, ///< Device-local, no CPU access (attachments, textures)
        Upload,  ///< CPU-to-GPU, persistently mapped (all Buffer writes)
        Readback ///< GPU-to-CPU, persistently mapped (pick/cull readback)
    };

    /** Opaque allocation token; its representation is private to
     * MemoryAllocator. */
    class MemoryAllocation
    {
        friend class MemoryAllocator;

      public:
        constexpr MemoryAllocation() = default;
        [[nodiscard]] constexpr explicit operator bool() const
        {
            return mHandle != nullptr;
        }

      private:
        void* mHandle = nullptr;
    };

    struct BufferAllocationInfo
    {
        void* mappedData   = nullptr;
        bool  hostCoherent = true;
    };

    /** @brief Result of a buffer allocation. */
    struct BufferAllocation
    {
        VkBuffer             buffer = {};
        MemoryAllocation     allocation {};
        BufferAllocationInfo info {};
    };

    /**
     * @brief Result of an image allocation.
     */
    struct ImageAllocation
    {
        VkImage          image = {};
        MemoryAllocation allocation {};
    };

    /**
     * @brief Thin RAII wrapper around the GPU memory allocator.
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

        void DestroyBuffer(VkBuffer buffer, MemoryAllocation allocation);
        void DestroyImage(VkImage image, MemoryAllocation allocation);

        void                Flush(MemoryAllocation allocation,
                                  VkDeviceSize     offset,
                                  VkDeviceSize     size);
        [[nodiscard]] void* Map(MemoryAllocation allocation);
        void                Unmap(MemoryAllocation allocation);

      private:
        struct Impl;
        Impl* mImpl = nullptr;
    };

} // namespace FREYA_NAMESPACE
