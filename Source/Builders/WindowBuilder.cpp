#include "Builders/WindowBuilder.hpp"

namespace FREYA_NAMESPACE
{

    Ref<Window> WindowBuilder::Build()
    {
        assert(SDL_Init(SDL_INIT_VIDEO) == 0 && "Failed to initialize SDL3");

        SDL_Vulkan_LoadLibrary(nullptr);

        constexpr auto windowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

        SDL_Window* window =
            SDL_CreateWindow(mTitle.c_str(), static_cast<int>(mWidth), static_cast<int>(mHeight), windowFlags);

        assert(window != nullptr && "Failed to create SDL3 Window");

        return MakeRef<Window>(
            window, mTitle, mWidth, mHeight, mVSync, mEventManager);
    }

} // namespace FREYA_NAMESPACE