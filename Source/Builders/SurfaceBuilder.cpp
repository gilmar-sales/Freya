#include "Freya/Builders/SurfaceBuilder.hpp"

#include "Freya/Core/Device.hpp"
#include "Freya/Core/Instance.hpp"
#include "Freya/Core/PhysicalDevice.hpp"

namespace FREYA_NAMESPACE
{

    Ref<Surface> SurfaceBuilder::Build()
    {
        mLogger->Assert(
            mInstance != nullptr,
            "Could not build 'fra::Surface' with an invalid 'fra::Instance'");

        VkSurfaceKHR cSurface;
        SDL_Vulkan_CreateSurface(mWindow, mInstance->Get(), nullptr, &cSurface);

        auto surface = vk::SurfaceKHR { cSurface };
        mLogger->Assert(surface, "Failed to create SDL3 surface.");

        mLogger->LogTrace("Building 'fra::Surface'.");

        return MakeRef<Surface>(mInstance, mPhysicalDevice, surface, mWindow);
    }

} // namespace FREYA_NAMESPACE
