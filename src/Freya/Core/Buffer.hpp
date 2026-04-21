#pragma once

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
        Vertex,   ///< Vertex buffer (device local)
        Index,    ///< Index buffer (device local)
        Uniform,  ///< Uniform buffer (device local)
        Instance, ///< Instance buffer (device local)
        Image     ///< Image buffer
    };

    /**
     * @brief Wrapper for Vulkan buffer with device memory management.
     *
     * Manages buffer lifecycle, binding to command buffers, and memory
     * operations. Supports staging, vertex, index, uniform, instance,
     * and image buffer usage types.
     *
     * @param device  Device reference
     * @param usage   Buffer usage type
     * @param size    Buffer size in bytes
     * @param buffer  Vulkan buffer handle
     * @param memory  Device memory handle
     */
    class Buffer
    {
      public:
        Buffer(const Ref<Device>&     device,
               const BufferUsage      usage,
               const std::uint64_t    size,
               const vk::Buffer       buffer,
               const vk::DeviceMemory memory) :
            mDevice(device), mBuffer(buffer), mMemory(memory), mUsage(usage),
            mSize(size)
        {
        }

        ~Buffer();

        /**
         * @brief Binds this buffer to the current command buffer.
         * @param commandPool Command pool with current command buffer
         * @note Uses usage type to determine binding (vertex/index/instance)
         */
        void Bind(const Ref<CommandPool>& commandPool) const;

        /**
         * @brief Returns the underlying buffer handle.
         */
        vk::Buffer& Get() { return mBuffer; }

        /**
         * @brief Returns the underlying device memory handle.
         */
        vk::DeviceMemory& GetMemory() { return mMemory; }

        /**
         * @brief Returns the buffer size in bytes.
         */
        [[nodiscard]] const std::uint64_t& GetSize() const { return mSize; }

        /**
         * @brief Copies data into the buffer memory.
         * @param data  Source data pointer
         * @param size  Size of data to copy
         * @param offset Offset into buffer memory (default 0)
         * @note Only copies if size fits within buffer and data is not null
         */
        void Copy(const void*   data,
                  std::uint64_t size,
                  std::uint64_t offset = 0);

      private:
        Ref<Device> mDevice;

        vk::Buffer       mBuffer;
        vk::DeviceMemory mMemory;
        BufferUsage      mUsage;
        std::uint64_t    mSize;
    };

} // namespace FREYA_NAMESPACE
