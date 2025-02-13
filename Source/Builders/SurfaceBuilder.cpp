#include "Freya/Builders/SurfaceBuilder.hpp"

#include "Freya/Core/Device.hpp"
#include "Freya/Core/Instance.hpp"
#include "Freya/Core/PhysicalDevice.hpp"

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

        return MakeRef<Surface>(mInstance, mPhysicalDevice, surface, mWindow);
    }

} // namespace FREYA_NAMESPACE
