#pragma once

#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/SwapChain.hpp"

namespace FREYA_NAMESPACE
{

    class CommandPoolBuilder
    {
      public:
        explicit CommandPoolBuilder(const Ref<Device>& device) :
            mDevice(device), mCount(0)
        {
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