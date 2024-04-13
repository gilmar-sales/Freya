#pragma once

#include "Core/CommandPool.hpp"
#include "Core/Device.hpp"
#include "Core/SwapChain.hpp"


namespace FREYA_NAMESPACE
{

    class CommandPoolBuilder
    {
      public:
        CommandPoolBuilder& SetDevice(std::shared_ptr<Device> device)
        {
            mDevice = device;
            return *this;
        }

        CommandPoolBuilder& SetSwapChain(std::shared_ptr<SwapChain> swapChain)
        {
            mSwapChain = swapChain;
            return *this;
        }

        std::shared_ptr<CommandPool> Build();

      private:
        std::shared_ptr<Device>    mDevice;
        std::shared_ptr<SwapChain> mSwapChain;
    };

} // namespace FREYA_NAMESPACE