#include "Freya/Builders/SurfaceBuilder.hpp"

namespace FREYA_NAMESPACE
{

    skr::Arc<Surface> SurfaceBuilder::Build()
    {
        mLogger->Assert(
            mInstance != nullptr,
            "Could not build 'fra::Surface' with an invalid 'fra::Instance'");

        VkSurfaceKHR cSurface;
        SDL_Vulkan_CreateSurface(mWindow->Get(),
                                 mInstance->Get(),
                                 nullptr,
                                 &cSurface);

        auto surface = vk::SurfaceKHR { cSurface };
        mLogger->Assert(surface, "Failed to create SDL3 surface.");

        mLogger->LogTrace("Building 'fra::Surface'.");

        auto fraSurface =
            skr::MakeArc<Surface>(mInstance, mPhysicalDevice, mWindow, surface);

        mFreyaOptions->frameCount =
            fraSurface->QueryFrameCountSupport(mFreyaOptions->frameCount);

        return fraSurface;
    }

} // namespace FREYA_NAMESPACE
