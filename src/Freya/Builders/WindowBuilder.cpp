#include "WindowBuilder.hpp"

#include "Freya/Internal/WindowNative.hpp"

#include <SDL3/SDL.h>

#include <memory>

namespace FREYA_NAMESPACE
{

    skr::Arc<Window> WindowBuilder::Build()
    {
        mLogger->LogTrace("Building 'fra::Window':");

        NativeWindowDesc desc {
            .title      = mFreyaOptions->title,
            .width      = mFreyaOptions->width,
            .height     = mFreyaOptions->height,
            .fullscreen = mFreyaOptions->fullscreen,
        };

        auto* nativeWindow = static_cast<SDL_Window*>(
            mPlatform->CreateNativeWindow(desc, *mFreyaOptions));

        mLogger->LogTrace("\tSize:{}x{}", mFreyaOptions->width,
                          mFreyaOptions->height);
        mLogger->LogTrace("\tVSync: {}", mFreyaOptions->vSync);
        mLogger->LogTrace(
            "\tFullscreen: {}",
            (bool) (SDL_GetWindowFlags(nativeWindow) & SDL_WINDOW_FULLSCREEN));

        auto impl          = std::make_unique<Window::Impl>();
        impl->eventManager = mEventManager;
        impl->freyaOptions = mFreyaOptions;
        impl->platform     = mPlatform;
        impl->logger       = mWindowLogger;
        impl->window       = nativeWindow;
        impl->running      = true;
        impl->deltaTime    = 0;

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

        mPlatform->AttachWindow(nativeWindow, impl.get());

        return skr::MakeArc<Window>(std::move(impl));
    }

} // namespace FREYA_NAMESPACE
