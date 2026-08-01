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
        LightServiceBuilder(const skr::Arc<Device>&       device,
                            const skr::Arc<FreyaOptions>& freyaOptions) :
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
        skr::Arc<LightService> Build()
        {
            return skr::MakeArc<LightService>(mDevice,
                                              mFreyaOptions->frameCount,
                                              mMaxLights);
        }

      private:
        skr::Arc<Device>       mDevice;
        skr::Arc<FreyaOptions> mFreyaOptions;
        std::uint32_t     mMaxLights = MAX_LIGHTS;
    };

} // namespace FREYA_NAMESPACE