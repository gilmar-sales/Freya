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

} // namespace FREYA_NAMESPACE
