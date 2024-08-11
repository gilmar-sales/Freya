#include "Builders/WindowBuilder.hpp"

namespace FREYA_NAMESPACE
{

    std::shared_ptr<Window> WindowBuilder::Build()
    {
        assert(SDL_Init(SDL_INIT_VIDEO) == 0 && "Failed to initialize SDL3");

        SDL_Vulkan_LoadLibrary(nullptr);

        auto windowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

        SDL_Window* window =
            SDL_CreateWindow(mTitle.c_str(), mWidth, mHeight, windowFlags);

        assert(window != nullptr && "Failed to create SDL3 Window");

        return std::make_shared<Window>(
            window, mTitle, mWidth, mHeight, mVSync, mEventManager);
    }

} // namespace FREYA_NAMESPACE