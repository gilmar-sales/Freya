#pragma once

#include "Core/Surface.hpp"

namespace FREYA_NAMESPACE
{
    class Instance;
    class PhysicalDevice;

    class SurfaceBuilder
    {
      public:
        SurfaceBuilder &SetWindow(SDL_Window *window)
        {
            mWindow = window;
            return *this;
        }

        SurfaceBuilder &SetInstance(Ref<Instance> instance)
        {
            mInstance = instance;
            return *this;
        }

        SurfaceBuilder &SetPhysicalDevice(Ref<PhysicalDevice> physicalDevice)
        {
            mPhysicalDevice = physicalDevice;
            return *this;
        }

        Ref<Surface> Build();

      private:
        SDL_Window *mWindow;
        Ref<Instance> mInstance;
        Ref<PhysicalDevice> mPhysicalDevice;

        std::uint32_t mImageCount;
        std::uint32_t mWidth;
        std::uint32_t mHeight;
    };

} // namespace FREYA_NAMESPACE
