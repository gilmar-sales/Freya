#pragma once

#include "Core/Buffer.hpp"
#include "Core/Device.hpp"

namespace FREYA_NAMESPACE
{

    class BufferBuilder
    {
      public:
        BufferBuilder(std::shared_ptr<Device> device)
            : mDevice(device), mData(nullptr), mSize(0), mUsage(BufferUsage::Staging)
        {
        }

        BufferBuilder &SetSize(std::uint32_t size)
        {
            mSize = size;

            return *this;
        }

        template <typename T>
        BufferBuilder &SetData(T *data)
        {
            mData = (void *) data;

            return *this;
        }

        BufferBuilder &SetUsage(BufferUsage usage)
        {
            mUsage = usage;
            return *this;
        }

        std::shared_ptr<Buffer> Build();

      private:
        std::shared_ptr<Device> mDevice;
        std::uint32_t mSize;
        void *mData;
        BufferUsage mUsage;
    };

} // namespace FREYA_NAMESPACE
