#include "SdlPlatform.hpp"

#include "Freya/Internal/WindowNative.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

namespace FREYA_NAMESPACE
{
    SdlPlatform::SdlPlatform(const skr::Arc<skr::Logger<SdlPlatform>>& logger) :
        mLogger(logger)
    {
        mLogger->Assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD),
                        "Failed to initialize SDL3");

        const auto vulkanLoad = SDL_Vulkan_LoadLibrary(nullptr);
        mLogger->LogWarning("Vulkan loaded: {}", vulkanLoad);
    }

    SdlPlatform::~SdlPlatform()
    {
        mWindowsById.clear();
        SDL_Vulkan_UnloadLibrary();
        SDL_Quit();
    }

    void* SdlPlatform::CreateNativeWindow(const NativeWindowDesc& desc,
                                          FreyaOptions&           options)
    {
        auto windowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE |
                           SDL_WINDOW_HIGH_PIXEL_DENSITY;

        options.title      = desc.title;
        options.width      = desc.width;
        options.height     = desc.height;
        options.fullscreen = desc.fullscreen;

        if (desc.fullscreen)
        {
            windowFlags |= SDL_WINDOW_FULLSCREEN;
        }
        else
        {
            const auto             displayId = SDL_GetPrimaryDisplay();
            const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(displayId);

            if (mode && options.width == static_cast<std::uint32_t>(mode->w) &&
                options.height == static_cast<std::uint32_t>(mode->h))
            {
                mLogger->LogWarning(
                    "Window size matches display resolution, "
                    "reducing resolution to avoid forced fullscreen");

                SDL_Rect usableBounds;
                if (SDL_GetDisplayUsableBounds(displayId, &usableBounds))
                {
                    options.width  = usableBounds.w;
                    options.height = usableBounds.h - 46;
                }
                else
                {
                    options.width -= 10;
                    options.height -= 60;
                }
            }
        }

        auto* window = SDL_CreateWindow(
            options.title.c_str(), static_cast<int>(options.width),
            static_cast<int>(options.height), windowFlags);

        mLogger->Assert(window != nullptr, "Failed to create SDL3 Window");

        int width  = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        options.width  = static_cast<std::uint32_t>(width);
        options.height = static_cast<std::uint32_t>(height);

        mLogger->LogTrace("Created native window {}x{} vsync={}", options.width,
                          options.height, options.vSync);

        return window;
    }

    void SdlPlatform::DestroyNativeWindow(void* nativeWindow)
    {
        if (!nativeWindow)
            return;

        auto* window = static_cast<SDL_Window*>(nativeWindow);
        DetachWindow(nativeWindow);
        SDL_DestroyWindow(window);
    }

    void SdlPlatform::AttachWindow(void* nativeWindow, void* windowToken)
    {
        if (!nativeWindow || !windowToken)
            return;

        const auto id = SDL_GetWindowID(static_cast<SDL_Window*>(nativeWindow));
        mWindowsById[id] = windowToken;
    }

    void SdlPlatform::DetachWindow(void* nativeWindow)
    {
        if (!nativeWindow)
            return;

        const auto id = SDL_GetWindowID(static_cast<SDL_Window*>(nativeWindow));
        mWindowsById.erase(id);
    }

    float SdlPlatform::GetDisplayContentScale(void* nativeWindow) const
    {
        if (!nativeWindow)
            return 1.0f;

        const SDL_DisplayID displayId =
            SDL_GetDisplayForWindow(static_cast<SDL_Window*>(nativeWindow));
        return SDL_GetDisplayContentScale(displayId);
    }

    void SdlPlatform::SetNativeEventObserver(const NativeEventObserver observer,
                                             void*                     user)
    {
        mEventObserver = observer;
        mEventUser     = user;
    }

    void SdlPlatform::PumpEvents()
    {
        SDL_Event sdlEvent;
        while (SDL_PollEvent(&sdlEvent))
        {
            if (mEventObserver)
                mEventObserver(&sdlEvent, mEventUser);

            std::uint32_t windowId = 0;
            switch (sdlEvent.type)
            {
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                case SDL_EVENT_WINDOW_MINIMIZED:
                case SDL_EVENT_WINDOW_RESTORED:
                case SDL_EVENT_WINDOW_RESIZED:
                    windowId = sdlEvent.window.windowID;
                    break;
                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP:
                    windowId = sdlEvent.key.windowID;
                    break;
                case SDL_EVENT_MOUSE_MOTION:
                    windowId = sdlEvent.motion.windowID;
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP:
                    windowId = sdlEvent.button.windowID;
                    break;
                case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                case SDL_EVENT_GAMEPAD_BUTTON_UP:
                case SDL_EVENT_GAMEPAD_ADDED:
                case SDL_EVENT_GAMEPAD_REMOVED:
                case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                    // Gamepad events are not window-scoped; fan out.
                    for (auto& [id, token] : mWindowsById)
                    {
                        (void) id;
                        Window::Impl::DispatchEvent(
                            static_cast<Window::Impl*>(token), sdlEvent);
                    }
                    continue;
                default:
                    continue;
            }

            const auto it = mWindowsById.find(windowId);
            if (it == mWindowsById.end())
                continue;

            Window::Impl::DispatchEvent(
                static_cast<Window::Impl*>(it->second), sdlEvent);
        }
    }

} // namespace FREYA_NAMESPACE
