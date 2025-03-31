#pragma once

#include "Freya/Core/Device.hpp"
#include "Freya/Core/Instance.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/Core/Window.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    class Instance;
    class PhysicalDevice;
    class Window;
    struct FreyaOptions;

    class SurfaceBuilder
    {
      public:
        SurfaceBuilder(const Ref<Instance>&                    instance,
                       const Ref<PhysicalDevice>&              physicalDevice,
                       const Ref<Window>&                      window,
                       const Ref<FreyaOptions>&                freyaOptions,
                       const Ref<skr::Logger<SurfaceBuilder>>& logger) :
            mInstance(instance), mPhysicalDevice(physicalDevice),
            mWindow(window), mFreyaOptions(freyaOptions), mLogger(logger)
        {
        }

        Ref<Surface> Build();

      private:
        Ref<skr::Logger<SurfaceBuilder>> mLogger;
        Ref<Window>                      mWindow;
        Ref<Instance>                    mInstance;
        Ref<PhysicalDevice>              mPhysicalDevice;
        Ref<FreyaOptions>                mFreyaOptions;
    };

} // namespace FREYA_NAMESPACE
