#pragma once

#include "Freya/Core/Device.hpp"
#include "Freya/Core/LightService.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Builder for creating LightService objects (internal DI).
     */
    class LightServiceBuilder
    {
      public:
        LightServiceBuilder(const skr::Arc<Device>&       device,
                            const skr::Arc<FreyaOptions>& freyaOptions) :
            mDevice(device), mFreyaOptions(freyaOptions)
        {
            (void) mDevice;
        }

        LightServiceBuilder& SetMaxLights(std::uint32_t maxLights)
        {
            mMaxLights = maxLights;
            return *this;
        }

        skr::Arc<LightService> Build(
            const skr::Arc<skr::ServiceProvider>& serviceProvider)
        {
            if (mMaxLights != kMaxLights)
                mFreyaOptions->maxLights = mMaxLights;
            return skr::MakeArc<LightService>(serviceProvider);
        }

      private:
        skr::Arc<Device>       mDevice;
        skr::Arc<FreyaOptions> mFreyaOptions;
        std::uint32_t          mMaxLights = kMaxLights;
    };

} // namespace FREYA_NAMESPACE
