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

        static auto cameraRotation = glm::vec3(0.0f, 0.0f, 0.0f);

        auto cameraMatrix = glm::mat4(1.0f);
        cameraMatrix      = glm::rotate(cameraMatrix,
                                        glm::radians(cameraRotation.x),
                                        glm::vec3(1.0f, 0.0f, 0.0f));
        cameraMatrix      = glm::rotate(cameraMatrix,
                                        glm::radians(cameraRotation.y),
                                        glm::vec3(0.0f, 1.0f, 0.0f));
        cameraMatrix      = glm::rotate(cameraMatrix,
                                        glm::radians(cameraRotation.z),
                                        glm::vec3(0.0f, 0.0f, 1.0f));

        static auto cameraPosition = glm::vec3(0.0f, 0.0f, -1.0f);
        auto        cameraForward =
            glm::vec3(glm::vec4(0.0f, 0.0f, 1.0f, 0.0) * cameraMatrix);
        auto cameraRight =
            glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), cameraForward));
        auto cameraUp = glm::normalize(glm::cross(cameraForward, cameraRight));

        static auto     cameraVelocity = glm::vec2(0.0f, 0.0f);
        static SDL_bool grab           = SDL_FALSE;

        static auto samples = false;

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_EVENT_QUIT:
                    mRunning = false;
                    break;
                case SDL_EVENT_WINDOW_RESIZED:
                    mWidth  = event.window.data1;
                    mHeight = event.window.data2;
                    //mRenderer->RebuildSwapChain();

                    break;
                case SDL_EVENT_WINDOW_MINIMIZED:
                    while (event.type != SDL_EVENT_WINDOW_RESTORED)
                    {
                        SDL_WaitEvent(&event);
                    }

                    mWidth  = event.window.data1;
                    mHeight = event.window.data2;
                    //mRenderer->RebuildSwapChain();
                    break;
                case SDL_EVENT_KEY_DOWN:
                    switch (event.key.keysym.scancode)
                    {
                        case SDL_SCANCODE_V:
                            mVSync = !mVSync;
                            //mRenderer->SetVSync(mVSync);
                            break;
                        case SDL_SCANCODE_P:
                            samples = !samples;
                            if (samples)
                            {
                                //mRenderer->SetSamples(vk::SampleCountFlagBits::e8);
                            }
                            else
                            {
                                //mRenderer->SetSamples(vk::SampleCountFlagBits::e1);
                            }
                            break;
                        case SDL_SCANCODE_M:

                            if (grab == SDL_TRUE)
                                grab = SDL_FALSE;
                            else
                                grab = SDL_TRUE;

                            SDL_SetRelativeMouseMode(grab);
                            break;
                        case SDL_SCANCODE_W:
                            cameraVelocity.y = 1.0f;
                            break;
                        case SDL_SCANCODE_S:
                            cameraVelocity.y = -1.0f;
                            break;
                        case SDL_SCANCODE_D:
                            cameraVelocity.x = -1.0f;
                            break;
                        case SDL_SCANCODE_A:
                            cameraVelocity.x = 1.0f;
                            break;
                        default:
                            break;
                    }
                    break;
                case SDL_EVENT_KEY_UP:
                    switch (event.key.keysym.scancode)
                    {
                        case SDL_SCANCODE_W:
                            cameraVelocity.y = 0.0f;
                            break;
                        case SDL_SCANCODE_S:
                            cameraVelocity.y = 0.0f;
                            break;
                        case SDL_SCANCODE_D:
                            cameraVelocity.x = 0.0f;
                            break;
                        case SDL_SCANCODE_A:
                            cameraVelocity.x = 0.0f;
                            break;
                        default:
                            break;
                    }
                    break;
                case SDL_EVENT_MOUSE_MOTION:
                    cameraRotation.y += event.motion.xrel * 10.0f * mDeltaTime;
                    cameraRotation.x -= event.motion.yrel * 10.0f * mDeltaTime;
                    break;
                default:
                    break;
            }
        }

        frames++;
    }

} // namespace FREYA_NAMESPACE
