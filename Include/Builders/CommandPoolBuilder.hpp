#pragma once

#include "Core/CommandPool.hpp"
#include "Core/Device.hpp"
#include "Core/SwapChain.hpp"

namespace FREYA_NAMESPACE
{

    class CommandPoolBuilder
    {
      public:
        explicit CommandPoolBuilder() :
            mCount(0) {}

        CommandPoolBuilder& SetDevice(const Ref<Device>& device)
        {
            mDevice = device;
            return *this;
        }

        CommandPoolBuilder& SetCount(const std::uint32_t count)
        {
            mCount = count;
            return *this;
        }

        Ref<CommandPool> Build();

      private:
        Ref<Device> mDevice;

        std::uint32_t mCount;
    };

} // namespace FREYA_NAMESPACE