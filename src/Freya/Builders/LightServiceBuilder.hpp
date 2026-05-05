#pragma once

#include "Freya/Core/Device.hpp"
#include "Freya/Core/LightService.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Builder for creating LightService objects.
     *
     * Provides fluent interface for configuring light service parameters
     * before construction.
     */
    class LightServiceBuilder
    {
      public:
        /**
         * @brief Constructs builder with required dependencies.
         * @param device     Vulkan device reference
         * @param freyaOptions Freya options containing frameCount
         */
        LightServiceBuilder(const Ref<Device>&       device,
                            const Ref<FreyaOptions>& freyaOptions) :
            mDevice(device), mFreyaOptions(freyaOptions)
        {
        }

        /**
         * @brief Sets the maximum number of lights.
         * @param maxLights Maximum light count (default: MAX_LIGHTS)
         * @return Reference to this for chaining
         */
        LightServiceBuilder& SetMaxLights(std::uint32_t maxLights)
        {
            mMaxLights = maxLights;
            return *this;
        }

        /**
         * @brief Builds and returns the LightService object.
         * @return Shared pointer to created LightService
         */
        Ref<LightService> Build()
        {
            return skr::MakeRef<LightService>(mDevice,
                                              mFreyaOptions->frameCount,
                                              mMaxLights);
        }

      private:
        Ref<Device>       mDevice;
        Ref<FreyaOptions> mFreyaOptions;
        std::uint32_t     mMaxLights = MAX_LIGHTS;
    };

} // namespace FREYA_NAMESPACE