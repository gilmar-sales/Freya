#pragma once

#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/SwapChain.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Builder for creating CommandPool objects.
     *
     * @param device Device reference
     */
    class CommandPoolBuilder
    {
      public:
        /**
         * @brief Constructs builder with device reference.
         * @param device Device reference
         */
        explicit CommandPoolBuilder(const Ref<Device>& device) :
            mDevice(device), mCount(0)
        {
        }

        /**
         * @brief Sets number of command buffers to allocate.
         * @param count Number of command buffers
         * @return Reference to this for chaining
         */
        CommandPoolBuilder& SetCount(const std::uint32_t count)
        {
            mCount = count;
            return *this;
        }

        /**
         * @brief Builds and returns the CommandPool object.
         * @return Shared pointer to created CommandPool
         */
        Ref<CommandPool> Build();

      private:
        Ref<Device> mDevice; ///< Device reference

        std::uint32_t mCount; ///< Number of command buffers to allocate
    };

} // namespace FREYA_NAMESPACE