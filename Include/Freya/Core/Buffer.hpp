#pragma once

namespace FREYA_NAMESPACE
{
    class Device;
    class CommandPool;

    enum class BufferUsage
    {
        Staging,
        Vertex,
        Index,
        Uniform,
        Instance,
        Image
    };

    class Buffer
    {
      public:
        Buffer(const Ref<Device>&     device,
               const BufferUsage      usage,
               const std::uint64_t    size,
               const vk::Buffer       buffer,
               const vk::DeviceMemory memory) :
            mDevice(device), mBuffer(buffer), mMemory(memory),
            mUsage(usage), mSize(size)
        {
        }

        ~Buffer();

        void Bind(const Ref<CommandPool>& commandPool) const;

        vk::Buffer& Get() { return mBuffer; }

        vk::DeviceMemory& GetMemory() { return mMemory; }

        [[nodiscard]] const std::uint64_t& GetSize() const { return mSize; }

        void Copy(const void* data, std::uint64_t size, std::uint64_t offset = 0);

      private:
        Ref<Device> mDevice;

        vk::Buffer       mBuffer;
        vk::DeviceMemory mMemory;
        BufferUsage      mUsage;
        std::uint64_t    mSize;
    };

} // namespace FREYA_NAMESPACE
