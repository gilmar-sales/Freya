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
        TexCoord,
        Uniform,
        Instance,
        Image
    };

    class Buffer
    {
      public:
        Buffer(Ref<Device> device,
               BufferUsage             usage,
               std::uint32_t           size,
               vk::Buffer              buffer,
               vk::DeviceMemory        memory) :
            mDevice(device),
            mUsage(usage), mBuffer(buffer), mMemory(memory),
            mSize(size)
        {
        }

        ~Buffer();

        void Bind(Ref<CommandPool> commandPool);

        vk::Buffer& Get() { return mBuffer; }

        vk::DeviceMemory& GetMemory() { return mMemory; }

        const std::uint32_t& GetSize() { return mSize; }

        void Copy(void* data, std::uint32_t size, std::uint32_t offset = 0);

      private:
        Ref<Device> mDevice;

        vk::Buffer       mBuffer;
        vk::DeviceMemory mMemory;
        BufferUsage      mUsage;
        std::uint32_t    mSize;
    };

} // namespace FREYA_NAMESPACE
