#include "Freya/Builders/WindowBuilder.hpp"

namespace FREYA_NAMESPACE
{

    Ref<Window> WindowBuilder::Build()
    {
        auto sdlInit = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);

        mLogger->Assert(sdlInit, "Failed to initialize SDL3");

        auto vulkanLoad = SDL_Vulkan_LoadLibrary(nullptr);

        mLogger->Assert(vulkanLoad, "Failed to load Vulkan");

        constexpr auto windowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

        auto window = SDL_CreateWindow(
            mTitle.c_str(),
            static_cast<int>(mWidth),
            static_cast<int>(mHeight),
            windowFlags);

        mLogger->Assert(window != nullptr, "Failed to create SDL3 Window");

        mLogger->LogTrace("Building 'fra::Window':");
        mLogger->LogTrace("\tSize:{}x{}", mWidth, mHeight);
        mLogger->LogTrace("\tVSync: {}", mVSync);

        return MakeRef<Window>(
            window,
            mTitle,
            mWidth,
            mHeight,
            mVSync,
            mEventManager);
    }

} // namespace FREYA_NAMESPACE