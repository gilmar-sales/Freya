#pragma once

#include "Freya/Core/MemoryAllocator.hpp"

namespace FREYA_NAMESPACE
{
    class Device;
    class CommandPool;

    /**
     * @brief Buffer usage type enumeration.
     */
    enum class BufferUsage
    {
        Staging,  ///< Transfer source buffer (host visible)
        Readback, ///< Transfer destination buffer (host visible)
        Vertex,   ///< Vertex buffer (device local)
        Index,    ///< Index buffer (device local)
        Uniform,  ///< Uniform buffer (device local)
        Instance, ///< Instance buffer (device local)
        Storage,  ///< Storage buffer (SSBO, host visible)
        Indirect, ///< Indirect draw commands (+ storage for compute fill)
        Image     ///< Image buffer
    };

    /**
     * @brief Wrapper for a VMA-backed Vulkan buffer.
     *
     * Memory is sub-allocated from MemoryAllocator blocks; the
     * buffer stays persistently mapped when host-visible.
     * Copy writes through the mapped pointer (no per-call
     * map/unmap) and flushes non-coherent ranges via VMA.
     *
     * @param device     Device reference (owns the VMA allocator)
     * @param usage      Buffer usage type
     * @param size       Buffer size in bytes
     * @param buffer     Vulkan buffer handle (VMA-owned)
     * @param allocation VMA allocation handle
     * @param info       VMA allocation info (mapped ptr, deviceMemory)
     */
    class Buffer
    {
      public:
        Buffer(const skr::Arc<Device>& device,
               const BufferUsage       usage,
               const std::uint64_t     size,
               const vk::Buffer        buffer,
               const MemoryAllocation     allocation,
               const BufferAllocationInfo info,
               const bool              hostCoherent = true) :
            mDevice(device), mBuffer(buffer), mAllocation(allocation),
             mInfo(info), mUsage(usage), mSize(size), mMapped(info.mappedData),
             mHostCoherent(info.hostCoherent && hostCoherent)
        {
        }

        ~Buffer();

        /**
         * @brief Binds this buffer to the current command buffer.
         * @param commandPool Command pool with current command buffer
         * @note Uses usage type to determine binding (vertex/index/instance)
         */
        void Bind(const skr::Arc<CommandPool>& commandPool) const;

        /**
         * @brief Returns the underlying buffer handle.
         */
        vk::Buffer& Get() { return mBuffer; }

        /**
         * @brief Returns the VMA allocation handle.
         */
        MemoryAllocation GetAllocation() const { return mAllocation; }

        /**
         * @brief Returns the VMA allocation info (deviceMemory, offset).
         */
        const BufferAllocationInfo& GetInfo() const { return mInfo; }

        /**
         * @brief Returns the buffer size in bytes.
         */
        [[nodiscard]] const std::uint64_t& GetSize() const { return mSize; }

        /**
         * @brief Persistent host mapping, or nullptr if not host-visible.
         */
        [[nodiscard]] void* GetMapped() const { return mMapped; }

        /**
         * @brief Copies data into the buffer memory.
         * @param data  Source data pointer
         * @param size  Size of data to copy
         * @param offset Offset into buffer memory (default 0)
         * @note Only copies if size fits within buffer and data is not null.
         *       Uses the persistent map when available.
         */
        void Copy(const void*   data,
                  std::uint64_t size,
                  std::uint64_t offset = 0);

      private:
        skr::Arc<Device> mDevice;

        vk::Buffer        mBuffer;
        MemoryAllocation     mAllocation {};
        BufferAllocationInfo mInfo {};
        BufferUsage       mUsage;
        std::uint64_t     mSize;
        void*             mMapped       = nullptr;
        bool              mHostCoherent = true;
    };

} // namespace FREYA_NAMESPACE
