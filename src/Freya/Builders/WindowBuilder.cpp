#include "WindowBuilder.hpp"

namespace FREYA_NAMESPACE
{

    Ref<Window> WindowBuilder::Build()
    {
        mLogger->LogTrace("Building 'fra::Window':");

        auto sdlInit = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);

        mLogger->Assert(sdlInit, "Failed to initialize SDL3");

        auto vulkanLoad = SDL_Vulkan_LoadLibrary(nullptr);

        mLogger->Assert(vulkanLoad, "Failed to load Vulkan");

        auto windowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

        if (mFreyaOptions->fullscreen)
        {
            windowFlags |= SDL_WINDOW_FULLSCREEN;
        }

        auto window = SDL_CreateWindow(
            mFreyaOptions->title.c_str(),
            static_cast<int>(mFreyaOptions->width),
            static_cast<int>(mFreyaOptions->height),
            windowFlags);

        mLogger->Assert(window != nullptr, "Failed to create SDL3 Window");

        mLogger->LogTrace("\tSize:{}x{}",
                          mFreyaOptions->width,
                          mFreyaOptions->height);
        mLogger->LogTrace("\tVSync: {}", mFreyaOptions->vSync);

        mLogger->LogTrace(
            "\tFullscreen: {}",
            (bool) (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));

        return skr::MakeRef<Window>(window, mEventManager, mFreyaOptions);
    }

} // namespace FREYA_NAMESPACE