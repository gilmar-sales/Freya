#pragma once

#include "Core/Buffer.hpp"
#include "Core/Device.hpp"

namespace FREYA_NAMESPACE
{

    class BufferBuilder
    {
      public:
        explicit BufferBuilder(std::shared_ptr<Device> device) :
            mDevice(device)
        {
        }

        BufferBuilder& SetSize(std::uint64_t size)
        {
            mSize = size;

            return *this;
        }

        template <typename T>
        BufferBuilder& SetData(T* data)
        {
            mData = (void*) data;

            return *this;
        }

        BufferBuilder& SetUsage(BufferUsage usage)
        {
            mUsage = usage;
            return *this;
        }

        std::shared_ptr<Buffer> Build();

      private:
        std::shared_ptr<Device> mDevice;

        std::uint64_t mSize  = 0;
        void*         mData  = nullptr;
        BufferUsage   mUsage = BufferUsage::Staging;
    };

} // namespace FREYA_NAMESPACE
