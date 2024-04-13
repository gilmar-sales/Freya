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

        SurfaceBuilder &SetInstance(std::shared_ptr<Instance> instance)
        {
            mInstance = instance;
            return *this;
        }

        SurfaceBuilder &SetPhysicalDevice(std::shared_ptr<PhysicalDevice> physicalDevice)
        {
            mPhysicalDevice = physicalDevice;
            return *this;
        }

        std::shared_ptr<Surface> Build();

      private:
        SDL_Window *mWindow;
        std::shared_ptr<Instance> mInstance;
        std::shared_ptr<PhysicalDevice> mPhysicalDevice;

        std::uint32_t mImageCount;
        std::uint32_t mWidth;
        std::uint32_t mHeight;
    };

} // namespace FREYA_NAMESPACE
