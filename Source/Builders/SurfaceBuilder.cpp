#include "Builders/SurfaceBuilder.hpp"

#include "Core/Device.hpp"
#include "Core/Instance.hpp"
#include "Core/PhysicalDevice.hpp"

namespace FREYA_NAMESPACE
{

    Ref<Surface> SurfaceBuilder::Build()
    {
        assert(mInstance.get() &&
               "Could not create fra::Surface with an invalid fra::Instance");

        VkSurfaceKHR cSurface;
        SDL_Vulkan_CreateSurface(mWindow, mInstance->Get(), nullptr, &cSurface);

        auto surface = vk::SurfaceKHR { cSurface };
        assert(surface && "Failed to create SDL2 surface.");

        return MakeRef<Surface>(mInstance, mPhysicalDevice, surface, mWidth, mHeight);
    }

} // namespace FREYA_NAMESPACE
