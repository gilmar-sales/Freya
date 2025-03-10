#pragma once

#include "Freya/Core/Surface.hpp"

namespace FREYA_NAMESPACE
{
    class Instance;
    class PhysicalDevice;

    class SurfaceBuilder
    {
      public:
        SurfaceBuilder(const Ref<skr::Logger>& logger) :
            mLogger(logger), mWindow(nullptr), mImageCount(0)
        {
        }

        SurfaceBuilder& SetWindow(SDL_Window* window)
        {
            mWindow = window;
            return *this;
        }

        SurfaceBuilder& SetInstance(const Ref<Instance>& instance)
        {
            mInstance = instance;
            return *this;
        }

        SurfaceBuilder& SetPhysicalDevice(
            const Ref<PhysicalDevice>& physicalDevice)
        {
            mPhysicalDevice = physicalDevice;
            return *this;
        }

        std::shared_ptr<Surface> Build();

      private:
        Ref<skr::Logger>    mLogger;
        SDL_Window*         mWindow;
        Ref<Instance>       mInstance;
        Ref<PhysicalDevice> mPhysicalDevice;

        std::uint32_t mImageCount;
    };

} // namespace FREYA_NAMESPACE
