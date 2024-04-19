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
        Instance
    };

    class Buffer
    {
      public:
        Buffer(std::shared_ptr<Device> device,
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

        void Bind(std::shared_ptr<CommandPool> commandPool);

        vk::Buffer& Get() { return mBuffer; }

        const std::uint32_t& GetSize() { return mSize; }

        void Copy(void* data, std::uint32_t size, std::uint32_t offset = 0);

      private:
        std::shared_ptr<Device> mDevice;

        vk::Buffer       mBuffer;
        vk::DeviceMemory mMemory;
        BufferUsage      mUsage;
        std::uint32_t    mSize;
    };

} // namespace FREYA_NAMESPACE
