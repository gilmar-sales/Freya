#include "WindowBuilder.hpp"

#include "Freya/Internal/WindowNative.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <memory>

namespace FREYA_NAMESPACE
{

    skr::Arc<Window> WindowBuilder::Build()
    {
        mLogger->LogTrace("Building 'fra::Window':");

        auto sdlInit = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);

        mLogger->Assert(sdlInit, "Failed to initialize SDL3");

        auto vulkanLoad = SDL_Vulkan_LoadLibrary(nullptr);

        mLogger->LogWarning("Vulkan loaded: {}", vulkanLoad);

        auto windowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE |
                           SDL_WINDOW_HIGH_PIXEL_DENSITY;

        if (mFreyaOptions->fullscreen)
        {
            windowFlags |= SDL_WINDOW_FULLSCREEN;
        }
        else
        {
            auto                   displayId = SDL_GetPrimaryDisplay();
            const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(displayId);

            if (mode && !mFreyaOptions->fullscreen &&
                mFreyaOptions->width == static_cast<uint32_t>(mode->w) &&
                mFreyaOptions->height == static_cast<uint32_t>(mode->h))
            {
                mLogger->LogWarning(
                    "Window size matches display resolution, "
                    "reducing resolution to avoid forced fullscreen");

                SDL_Rect usableBounds;
                if (SDL_GetDisplayUsableBounds(displayId, &usableBounds))
                {
                    mFreyaOptions->width  = usableBounds.w;
                    mFreyaOptions->height = usableBounds.h - 46;
                }
                else
                {
                    mFreyaOptions->width -= 10;
                    mFreyaOptions->height -= 60;
                }
            }
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

        auto impl          = std::make_unique<Window::Impl>();
        impl->eventManager = mEventManager;
        impl->freyaOptions = mFreyaOptions;
        impl->logger       = mWindowLogger;
        impl->window       = window;
        impl->running      = true;
        impl->deltaTime    = 0;

        int width  = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        mFreyaOptions->width  = static_cast<std::uint32_t>(width);
        mFreyaOptions->height = static_cast<std::uint32_t>(height);

        auto       gamepadCount = 0;
        const auto gamepadIds   = SDL_GetGamepads(&gamepadCount);
        for (int i = 0; i < gamepadCount; ++i)
        {
            if (SDL_IsGamepad(gamepadIds[i]))
            {
                if (auto* controller = SDL_OpenGamepad(gamepadIds[i]))
                    impl->gamepads.push_back(controller);
            }
        }

        return skr::MakeArc<Window>(std::move(impl));
    }

} // namespace FREYA_NAMESPACE
