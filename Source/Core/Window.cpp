#include "Core/Window.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_vulkan.h>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

#include <sstream>

namespace FREYA_NAMESPACE
{
    Window::~Window()
    {
        SDL_DestroyWindow(mWindow);
        SDL_Vulkan_UnloadLibrary();
        SDL_Quit();
    }

    void Window::Update()
    {
        static unsigned frames          = 0;
        static auto     previousCounter = SDL_GetPerformanceCounter();
        static double   secondTime      = 0;

        auto currentCount = SDL_GetPerformanceCounter();

        mDeltaTime = (double) (currentCount - previousCounter) /
                     (double) SDL_GetPerformanceFrequency();
        secondTime += mDeltaTime;
        previousCounter = currentCount;

        pollEvents();

        if (secondTime >= 1.0f)
        {
            secondTime       = 0;
            auto titleStream = std::stringstream();

            titleStream << mTitle << " - " << frames << " FPS";
            frames = 0;
            SDL_SetWindowTitle(mWindow, titleStream.str().c_str());
        }

        frames++;
    }

    void Window::pollEvents()
    {
        SDL_Event sdlEvent;

        while (SDL_PollEvent(&sdlEvent))
        {
            switch (sdlEvent.type)
            {
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {

                    mRunning = false;
                    break;
                }

                case SDL_EVENT_WINDOW_RESIZED: {
                    auto resizeEvent   = WindowResizeEvent {};
                    resizeEvent.width  = sdlEvent.window.data1;
                    resizeEvent.height = sdlEvent.window.data2;
                    mEventManager->Send(resizeEvent);
                    break;
                }

                case SDL_EVENT_KEY_DOWN: {
                    auto keyEvent = KeyPressedEvent {};
                    keyEvent.key  = (KeyCode) sdlEvent.key.scancode;
                    mEventManager->Send(keyEvent);
                    break;
                }

                case SDL_EVENT_KEY_UP: {
                    auto keyEvent = KeyReleasedEvent {};
                    keyEvent.key  = (KeyCode) sdlEvent.key.scancode;
                    mEventManager->Send(keyEvent);
                    break;
                }

                default:
                    break;
            }
        };
    }

} // namespace FREYA_NAMESPACE
