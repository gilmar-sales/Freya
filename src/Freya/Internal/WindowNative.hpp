#pragma once

#include "Freya/Core/IPlatform.hpp"
#include "Freya/Core/Window.hpp"

#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>

namespace FREYA_NAMESPACE
{
    struct Window::Impl
    {
        skr::Arc<EventManager>        eventManager;
        skr::Arc<FreyaOptions>        freyaOptions;
        skr::Arc<IPlatform>           platform;
        skr::Arc<skr::Logger<Window>> logger;

        std::vector<SDL_Gamepad*> gamepads;
        SDL_Window*               window    = nullptr;
        bool                      running   = true;
        float                     deltaTime = 0;

        unsigned frames            = 0;
        Uint64   previousCounter   = 0;
        double   secondTime        = 0;
        bool     timingInitialized = false;

        static void DispatchEvent(Impl* impl, const SDL_Event& sdlEvent);
        void        handleEvent(const SDL_Event& sdlEvent);
    };

    struct WindowNative
    {
        static SDL_Window* Get(const Window& window);
    };
} // namespace FREYA_NAMESPACE
