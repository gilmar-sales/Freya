#pragma once

#include "Freya/Core/Window.hpp"

#include <SDL3/SDL.h>

#include <vector>

namespace FREYA_NAMESPACE
{
    struct Window::Impl
    {
        skr::Arc<EventManager>        eventManager;
        skr::Arc<FreyaOptions>        freyaOptions;
        skr::Arc<skr::Logger<Window>> logger;

        std::vector<SDL_Gamepad*> gamepads;
        SDL_Window*               window    = nullptr;
        bool                      running   = true;
        float                     deltaTime = 0;

        void pollEvents();
    };

    struct WindowNative
    {
        static SDL_Window* Get(const Window& window);
    };
} // namespace FREYA_NAMESPACE
