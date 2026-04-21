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

    /**
     * @brief Builder for creating Surface objects from SDL3 window.
     *
     * @param instance      Instance reference
     * @param physicalDevice Physical device reference
     * @param window         Window reference
     * @param freyaOptions   Freya options reference
     * @param logger         Logger reference
     */
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

        /**
         * @brief Builds and returns the Surface object.
         * @return Shared pointer to created Surface
         */
        Ref<Surface> Build();

      private:
        Ref<skr::Logger<SurfaceBuilder>> mLogger;   ///< Logger reference
        Ref<Window>                      mWindow;   ///< Window reference
        Ref<Instance>                    mInstance; ///< Instance reference
        Ref<PhysicalDevice> mPhysicalDevice; ///< Physical device reference
        Ref<FreyaOptions>   mFreyaOptions;   ///< Freya options reference
    };

} // namespace FREYA_NAMESPACE
